/******************************************************************************
 * This file is part of dirtsand.                                             *
 *                                                                            *
 * dirtsand is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU Affero General Public License as             *
 * published by the Free Software Foundation, either version 3 of the         *
 * License, or (at your option) any later version.                            *
 *                                                                            *
 * dirtsand is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU Affero General Public License for more details.                        *
 *                                                                            *
 * You should have received a copy of the GNU Affero General Public License   *
 * along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.          *
 ******************************************************************************/

#include "AuthServer_Private.h"
#include "GameServ/GameServer.h"
#include "SDL/DescriptorDb.h"
#include "settings.h"
#include "errors.h"
#include <string_theory/codecs>
#include <string_theory/format>
#include <unordered_map>
#include <chrono>

std::thread s_authDaemonThread;
DS::MsgChannel s_authChannel;
PGconn* s_postgres;
bool s_restrictLogins = false;
extern uint32_t s_allPlayers;
std::unordered_map<ST::string, SDL::State, ST::hash_i, ST::equal_i> s_globalStates;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

static inline void check_postgres()
{
    if (PQstatus(s_postgres) == CONNECTION_BAD)
        PQreset(s_postgres);
    DS_DASSERT(PQstatus(s_postgres) == CONNECTION_OK);
}

void dm_auth_addacct(Auth_AddAcct* msg)
{
    check_postgres();

    PostgresStrings<3> qparms;
    qparms.set(0, msg->m_acctInfo.m_acctName);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT idx, \"AcctUuid\" FROM auth.\"Accounts\""
            "    WHERE LOWER(\"Login\")=LOWER($1)",
            1, 0, qparms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }

    if (PQntuples(result) == 0) {
        PQclear(result);

        ST::char_buffer pwBuf = msg->m_acctInfo.m_password.to_utf8();
        DS::ShaHash pwHash = DS::ShaHash::Sha1(pwBuf.data(), pwBuf.size());
        PostgresStrings<3> iparms;
        iparms.set(0, gen_uuid().toString());
        iparms.set(1, pwHash.toString());
        iparms.set(2, msg->m_acctInfo.m_acctName);
        PGresult* result = PQexecParams(s_postgres,
                "INSERT INTO auth.\"Accounts\""
                "    (\"AcctUuid\", \"PassHash\", \"Login\", \"AcctFlags\", \"BillingType\")"
                "    VALUES ($1, $2, $3, 0, 1)"
                "    returning idx",
                3, 0, iparms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            SEND_REPLY(msg, DS::e_NetInternalError);
            return;
        }
        DS_DASSERT(PQntuples(result) == 1);
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetSuccess);
    } else {
        fprintf(stderr, "Error: Account already exists (ID %s; UUID %s)\n",
                PQgetvalue(result, 0, 0), PQgetvalue(result, 0, 1));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetAccountAlreadyExists);
    }
}

void dm_auth_shutdown()
{
    {
        std::lock_guard<std::mutex> authClientGuard(s_authClientMutex);
        for (auto client_iter = s_authClients.begin(); client_iter != s_authClients.end(); ++client_iter)
            DS::CloseSock((*client_iter)->m_sock);
    }

    bool complete = false;
    for (int i=0; i<50 && !complete; ++i) {
        s_authClientMutex.lock();
        size_t alive = s_authClients.size();
        s_authClientMutex.unlock();
        if (alive == 0)
            complete = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!complete)
        fputs("[Auth] Clients didn't die after 5 seconds!\n", stderr);

    PQfinish(s_postgres);
    s_globalStates.clear();
}

void dm_auth_login(Auth_LoginInfo* info)
{
    check_postgres();

#ifdef DEBUG
    printf("[Auth] Login U:%s P:%s T:%s O:%s\n",
           info->m_acctName.c_str(), info->m_passHash.toString().c_str(),
           info->m_token.c_str(), info->m_os.c_str());
#endif

    // Reset UUID in case authentication fails
    AuthServer_Private* client = reinterpret_cast<AuthServer_Private*>(info->m_client);
    client->m_acctUuid = DS::Uuid();

    PostgresStrings<1> parm;
    parm.set(0, info->m_acctName);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT \"PassHash\", \"AcctUuid\", \"AcctFlags\", \"BillingType\""
            "    FROM auth.\"Accounts\""
            "    WHERE LOWER(\"Login\")=LOWER($1)",
            1, 0, parm.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(info, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) == 0) {
        printf("[Auth] %s: Account %s does not exist\n",
               DS::SockIpAddress(info->m_client->m_sock).c_str(),
               info->m_acctName.c_str());
        PQclear(result);
        // This should be NetAccountNotFound, but that's technically a
        // security flaw...
        SEND_REPLY(info, DS::e_NetAuthenticationFailed);
        return;
    }
#ifdef DEBUG
    if (PQntuples(result) != 1) {
        PQclear(result);
        DS_PASSERT(0);
    }
#endif

    DS::ShaHash passhash = PQgetvalue(result, 0, 0);
    if (info->m_acctName.find("@") != -1 && info->m_acctName.find("@gametap") == -1) {
        DS::ShaHash challengeHash = DS::BuggyHashLogin(passhash,
                client->m_serverChallenge, info->m_clientChallenge);
        if (challengeHash != info->m_passHash) {
            printf("[Auth] %s: Failed login to account %s\n",
                   DS::SockIpAddress(info->m_client->m_sock).c_str(),
                   info->m_acctName.c_str());
            PQclear(result);
            SEND_REPLY(info, DS::e_NetAuthenticationFailed);
            return;
        }
    } else {
        // In this case, the Sha1 hash is Big Endian...  Yeah, really...
        info->m_passHash.swapBytes();
        if (passhash != info->m_passHash) {
            printf("[Auth] %s: Failed login to account %s\n",
                   DS::SockIpAddress(info->m_client->m_sock).c_str(),
                   info->m_acctName.c_str());
            PQclear(result);
            SEND_REPLY(info, DS::e_NetAuthenticationFailed);
            return;
        }
    }

    client->m_acctUuid = DS::Uuid(PQgetvalue(result, 0, 1));
    client->m_acctFlags = strtoul(PQgetvalue(result, 0, 2), 0, 10);
    info->m_billingType = strtoul(PQgetvalue(result, 0, 3), 0, 10);
    printf("[Auth] %s logged in as %s {%s}\n",
           DS::SockIpAddress(info->m_client->m_sock).c_str(),
           info->m_acctName.c_str(), client->m_acctUuid.toString().c_str());
    PQclear(result);

    // Avoid fetching the players for banned dudes
    if (client->m_acctFlags & DS::e_AcctBanned) {
        SEND_REPLY(info, DS::e_NetAccountBanned);
        return;
    } else if (s_restrictLogins && !(client->m_acctFlags & (DS::e_AcctAdmin | DS::e_AcctBetaTester))) {
        SEND_REPLY(info, DS::e_NetLoginDenied);
        return;
    }

    // Get list of players
    ST::string uuidString = client->m_acctUuid.toString();
    parm.m_values[0] = uuidString.c_str();
    result = PQexecParams(s_postgres,
            "SELECT \"PlayerIdx\", \"PlayerName\", \"AvatarShape\", \"Explorer\""
            "    FROM auth.\"Players\""
            "    WHERE \"AcctUuid\"=$1",
            1, 0, parm.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(info, DS::e_NetInternalError);
        return;
    }
    info->m_players.resize(PQntuples(result));
    for (size_t i = 0; i < info->m_players.size(); ++i) {
        info->m_players[i].m_playerId = strtoul(PQgetvalue(result, i, 0), 0, 10);
        info->m_players[i].m_playerName = PQgetvalue(result, i, 1);
        info->m_players[i].m_avatarModel = PQgetvalue(result, i, 2);
        info->m_players[i].m_explorer = strtoul(PQgetvalue(result, i, 3), 0, 10);
    }
    PQclear(result);

    SEND_REPLY(info, DS::e_NetSuccess);
}

void dm_auth_bcast_node(uint32_t nodeIdx, const DS::Uuid& revision)
{
    DS::BufferStream* msg = new DS::BufferStream(nullptr, 20); // Node ID, Revision Uuid
    msg->write<uint32_t>(nodeIdx);
    msg->writeBytes(revision.m_bytes, 16);

    std::lock_guard<std::mutex> guard(s_authClientMutex);
    for (auto it = s_authClients.begin(); it != s_authClients.end(); ++it) {
        AuthServer_Private* client = *it;
        if (!(v_has_node(client->m_ageNodeId, nodeIdx) || v_has_node(client->m_player.m_playerId, nodeIdx)))
            continue;
        msg->ref();
        client->m_broadcast.putMessage(e_AuthToCli_VaultNodeChanged, msg);
    }
    msg->unref();
}

void dm_auth_bcast_ref(const DS::Vault::NodeRef& ref)
{
    DS::BufferStream* msg = new DS::BufferStream(nullptr, 12); // Parent, Child, Owner
    msg->write<uint32_t>(ref.m_parent);
    msg->write<uint32_t>(ref.m_child);
    msg->write<uint32_t>(ref.m_owner);

    std::lock_guard<std::mutex> guard(s_authClientMutex);
    for (auto it = s_authClients.begin(); it != s_authClients.end(); ++it) {
        AuthServer_Private* client = *it;
        if (!(v_has_node(client->m_ageNodeId, ref.m_parent) || v_has_node(client->m_player.m_playerId, ref.m_parent)))
            continue;
        msg->ref();
        client->m_broadcast.putMessage(e_AuthToCli_VaultNodeAdded, msg);
    }
    msg->unref();
}

void dm_auth_bcast_unref(const DS::Vault::NodeRef& ref)
{
    DS::BufferStream* msg = new DS::BufferStream(nullptr, 8); // Parent, Child
    msg->write<uint32_t>(ref.m_parent);
    msg->write<uint32_t>(ref.m_child);

    std::lock_guard<std::mutex> guard(s_authClientMutex);
    for (auto it = s_authClients.begin(); it != s_authClients.end(); ++it) {
        AuthServer_Private* client = *it;
        if (!(v_has_node(client->m_ageNodeId, ref.m_parent) || v_has_node(client->m_player.m_playerId, ref.m_parent)))
            continue;
        msg->ref();
        client->m_broadcast.putMessage(e_AuthToCli_VaultNodeRemoved, msg);
    }
    msg->unref();
}

void dm_auth_disconnect(Auth_ClientMessage* msg)
{
    AuthServer_Private* client = reinterpret_cast<AuthServer_Private*>(msg->m_client);
    if (client->m_player.m_playerId) {
        // Mark player as offline
        check_postgres();
        PostgresStrings<2> parms;
        parms.set(0, DS::Vault::e_NodePlayerInfo);
        parms.set(1, client->m_player.m_playerId);
        PGresult* result = PQexecParams(s_postgres,
                "UPDATE vault.\"Nodes\" SET"
                "    \"Int32_1\"=0, \"String64_1\"='',"
                "    \"Uuid_1\"='00000000-0000-0000-0000-000000000000'"
                "    WHERE \"NodeType\"=$1 AND \"Uint32_1\"=$2"
                "    RETURNING idx",
                2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            // This doesn't block continuing...
        }
        if (PQntuples(result) == 1) {
            uint32_t nodeid = strtoul(PQgetvalue(result, 0, 0), 0, 10);
            dm_auth_bcast_node(nodeid, gen_uuid());
        } else {
            fprintf(stderr, "%s:%d:\n    Could not get PlayerInfoNode idx on disconnect",
                    __FILE__, __LINE__);
            // This doesn't block continuing
        }
        PQclear(result);
    }
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_setPlayer(Auth_ClientMessage* msg)
{
    check_postgres();

    AuthServer_Private* client = reinterpret_cast<AuthServer_Private*>(msg->m_client);
    PostgresStrings<2> parms;
    parms.set(0, client->m_acctUuid.toString());
    parms.set(1, client->m_player.m_playerId);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT \"PlayerName\", \"AvatarShape\", \"Explorer\""
            "    FROM auth.\"Players\""
            "    WHERE \"AcctUuid\"=$1 AND \"PlayerIdx\"=$2",
            2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        client->m_player.m_playerId = 0;
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) == 0) {
        printf("[Auth] {%s} requested invalid player ID (%u)\n",
               client->m_acctUuid.toString().c_str(),
               client->m_player.m_playerId);
        PQclear(result);
        client->m_player.m_playerId = 0;
        SEND_REPLY(msg, DS::e_NetPlayerNotFound);
        return;
    }

#ifdef DEBUG
    if (PQntuples(result) != 1) {
        PQclear(result);
        client->m_player.m_playerId = 0;
        DS_PASSERT(0);
    }
#endif

    {
        std::lock_guard<std::mutex> authClientGuard(s_authClientMutex);
        for (auto client_iter = s_authClients.begin(); client_iter != s_authClients.end(); ++client_iter) {
            if (client != *client_iter && (*client_iter)->m_player.m_playerId == client->m_player.m_playerId) {
                printf("[Auth] {%s} requested already-active player (%u)\n",
                    client->m_acctUuid.toString().c_str(),
                    client->m_player.m_playerId);
                PQclear(result);
                client->m_player.m_playerId = 0;
                SEND_REPLY(msg, DS::e_NetLoggedInElsewhere);
                return;
            }
        }
    }

    client->m_player.m_playerName = PQgetvalue(result, 0, 0);
    client->m_player.m_avatarModel = PQgetvalue(result, 0, 1);
    client->m_player.m_explorer = strtoul(PQgetvalue(result, 0, 2), 0, 10);
    PQclear(result);

    // Mark player as online
    parms.set(0, DS::Vault::e_NodePlayerInfo);
    parms.set(1, client->m_player.m_playerId);
    result = PQexecParams(s_postgres,
            "UPDATE vault.\"Nodes\" SET"
            "    \"Int32_1\"=1, \"String64_1\"='Lobby',"
            "    \"Uuid_1\"='00000000-0000-0000-0000-000000000000'"
            "    WHERE \"NodeType\"=$1 AND \"Uint32_1\"=$2"
            "    RETURNING idx",
            2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        // This doesn't block continuing...
    }
    if (PQntuples(result) == 1) {
        uint32_t nodeid = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        dm_auth_bcast_node(nodeid, gen_uuid());
    } else {
        fprintf(stderr, "%s:%d:\n    Could not get PlayerInfoNode idx on setPlayer",
                __FILE__, __LINE__);
        // This doesn't block continuing
    }
    PQclear(result);

    printf("[Auth] {%s} signed in as %s (%u)\n",
           client->m_acctUuid.toString().c_str(),
           client->m_player.m_playerName.c_str(),
           client->m_player.m_playerId);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_createPlayer(Auth_PlayerCreate* msg)
{
    if (msg->m_player.m_avatarModel != "male" && msg->m_player.m_avatarModel != "female") {
        // Cheater!
        msg->m_player.m_avatarModel = "male";
    }

    // Check for existing player
    PostgresStrings<1> sparms;
    sparms.set(0, msg->m_player.m_playerName);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT idx FROM auth.\"Players\""
            "    WHERE \"PlayerName\"=$1",
            1, 0, sparms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) != 0) {
        fprintf(stderr, "[Auth] %s: Player %s already exists!\n",
                DS::SockIpAddress(msg->m_client->m_sock).c_str(),
                msg->m_player.m_playerName.c_str());
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetPlayerAlreadyExists);
        return;
    }
    PQclear(result);

    AuthServer_Private* client = reinterpret_cast<AuthServer_Private*>(msg->m_client);
    std::tuple<uint32_t, uint32_t, uint32_t> player =
        v_create_player(client->m_acctUuid, msg->m_player);
    msg->m_player.m_playerId = std::get<0>(player);
    if (msg->m_player.m_playerId == 0)
        SEND_REPLY(msg, DS::e_NetInternalError);

    // Tell neighborhood about its new member
    if (v_ref_node(std::get<2>(player), std::get<1>(player), std::get<0>(player)))
        dm_auth_bcast_ref({std::get<2>(player), std::get<1>(player), std::get<0>(player), 0});

    // Add new player to AllPlayers
    if (v_ref_node(s_allPlayers, std::get<1>(player), 0))
        dm_auth_bcast_ref({s_allPlayers, std::get<1>(player), 0, 0});

    PostgresStrings<5> iparms;
    iparms.set(0, client->m_acctUuid.toString());
    iparms.set(1, msg->m_player.m_playerId);
    iparms.set(2, msg->m_player.m_playerName);
    iparms.set(3, msg->m_player.m_avatarModel);
    iparms.set(4, msg->m_player.m_explorer);
    result = PQexecParams(s_postgres,
            "INSERT INTO auth.\"Players\""
            "    (\"AcctUuid\", \"PlayerIdx\", \"PlayerName\", \"AvatarShape\", \"Explorer\")"
            "    VALUES ($1, $2, $3, $4, $5)"
            "    RETURNING idx",
            5, 0, iparms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    DS_DASSERT(PQntuples(result) == 1);
    PQclear(result);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_deletePlayer(Auth_PlayerDelete* msg)
{
    AuthServer_Private* client = reinterpret_cast<AuthServer_Private*>(msg->m_client);

#ifdef DEBUG
    printf("[Auth] {%s} requesting deletion of PlayerId (%d)\n",
            client->m_acctUuid.toString().c_str(),
            msg->m_playerId);
#endif

    // Check for existing player

    PostgresStrings<2> sparms;
    sparms.set(0, client->m_acctUuid.toString());
    sparms.set(1, msg->m_playerId);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT idx FROM auth.\"Players\""
            "    WHERE \"AcctUuid\"=$1 AND \"PlayerIdx\"=$2",
            2, 0, sparms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) == 0) {
        fprintf(stderr, "[Auth] %s: PlayerId %d doesn't exist!\n",
                DS::SockIpAddress(msg->m_client->m_sock).c_str(),
                msg->m_playerId);
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetPlayerNotFound);
        return;
    }
    PQclear(result);

    PostgresStrings<1> dparms;
    dparms.set(0, msg->m_playerId);
    result = PQexecParams(s_postgres,
            "DELETE FROM auth.\"Players\""
            "    WHERE \"PlayerIdx\"=$1",
            1, 0, dparms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres DELETE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    PQclear(result);

    // Find PlayerInfo and remove all refs to it
    sparms.set(0, msg->m_playerId);
    sparms.set(1, DS::Vault::e_NodePlayerInfo);
    result = PQexecParams(s_postgres,
                          "SELECT idx FROM vault.\"Nodes\""
                          "    WHERE \"Uint32_1\" = $1"
                          "    AND \"NodeType\" = $2",
                          2, 0, sparms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    DS_PASSERT(PQntuples(result) == 1);
    uint32_t playerInfo = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);

    dparms.set(0, playerInfo);
    result = PQexecParams(s_postgres,
                          "DELETE FROM vault.\"NodeRefs\""
                          "    WHERE \"ChildIdx\" = $1",
                          1, 0, dparms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d\n    Postgres DELETE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    PQclear(result);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_createAge(Auth_AgeCreate* msg)
{
    std::tuple<uint32_t, uint32_t> ageNodes;
    PostgresStrings<2> params;
    params.set(0, msg->m_age.m_ageId.toString());
    PGresult* result = PQexecParams(s_postgres,
            "SELECT idx FROM vault.\"Nodes\""
            "   WHERE \"Uuid_1\"=$1 AND \"NodeType\"='3'",
            1, 0, params.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) == 1) {
        std::get<0>(ageNodes) = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);
        PGresult* result = PQexecParams(s_postgres,
                "SELECT idx FROM vault.\"Nodes\""
                "   WHERE \"Uuid_1\"=$1 AND \"NodeType\"='33'",
                1, 0, params.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d\n    Postgres SELECT error: %s\n",
            __FILE__, __LINE__, PQerrorMessage(s_postgres));
            SEND_REPLY(msg, DS::e_NetInternalError);
            return;
        }
        if (PQntuples(result) == 1) {
            std::get<1>(ageNodes) = strtoul(PQgetvalue(result, 0, 0), 0, 10);
            PQclear(result);
        } else {
            fprintf(stderr, "%s:%d\n    Got age but not age info? WTF?\n",
            __FILE__, __LINE__);
            SEND_REPLY(msg, DS::e_NetInternalError);
            return;
        }
    }
    else {
        ageNodes = v_create_age(msg->m_age, 0);
    }
    if (std::get<0>(ageNodes) == 0)
        SEND_REPLY(msg, DS::e_NetInternalError);

    msg->m_ageIdx = std::get<0>(ageNodes);
    msg->m_infoIdx = std::get<1>(ageNodes);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_findAge(Auth_GameAge* msg)
{
#ifdef DEBUG
    printf("[Auth] %s Requesting game server {%s} %s\n",
           DS::SockIpAddress(msg->m_client->m_sock).c_str(),
           msg->m_instanceId.toString().c_str(), msg->m_name.c_str());
#endif

    PostgresStrings<4> parms;
    parms.set(0, msg->m_instanceId.toString());
    PGresult* result = PQexecParams(s_postgres,
            "SELECT idx, \"AgeIdx\", \"DisplayName\" FROM game.\"Servers\""
            "    WHERE \"AgeUuid\"=$1",
            1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    ST::string ageDesc;
    if (PQntuples(result) == 0) {
        PQclear(result);
        parms.set(0, msg->m_instanceId.toString());
        parms.set(1, msg->m_name);
        result = PQexecParams(s_postgres,
                "INSERT INTO game.\"Servers\""
                "    (\"AgeUuid\", \"AgeFilename\", \"DisplayName\", \"AgeIdx\", \"SdlIdx\", \"Temporary\")"
                "    VALUES ($1, $2, $2, 0, 0, 't')"
                "    RETURNING idx, \"AgeIdx\", \"DisplayName\"",
                2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return;
        }
    }
    DS_DASSERT(PQntuples(result) == 1);
    msg->m_ageNodeIdx = strtoul(PQgetvalue(result, 0, 1), 0, 10);
    msg->m_mcpId = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    msg->m_serverAddress = DS::GetAddress4(DS::Settings::GameServerAddress().c_str());
    ageDesc = PQgetvalue(result, 0, 2);
    PQclear(result);

    // Update the player info to show up in the age
    parms.set(0, DS::Vault::e_NodePlayerInfo);
    parms.set(1, reinterpret_cast<AuthServer_Private*>(msg->m_client)->m_player.m_playerId);
    parms.set(2, ageDesc);
    parms.set(3, msg->m_instanceId.toString());
    result = PQexecParams(s_postgres,
            "UPDATE vault.\"Nodes\" SET"
            "    \"String64_1\"=$3, \"Uuid_1\"=$4"
            "    WHERE \"NodeType\"=$1 AND \"Uint32_1\"=$2"
            "    RETURNING idx",
            4, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        // This doesn't block continuing...
    }
    if (PQntuples(result) == 1) {
        uint32_t nodeid = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        dm_auth_bcast_node(nodeid, gen_uuid());
    } else {
        fprintf(stderr, "%s:%d:\n    Could not get PlayerInfoNode idx on findAge",
                __FILE__, __LINE__);
        // This doesn't block continuing
    }
    PQclear(result);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_get_public(Auth_PubAgeRequest* msg)
{
    if (v_find_public_ages(msg->m_agename, msg->m_ages))
        SEND_REPLY(msg, DS::e_NetSuccess);
    else
        SEND_REPLY(msg, DS::e_NetInternalError);
}

uint32_t dm_auth_set_public(uint32_t nodeid)
{
    PostgresStrings<3> parms;
    parms.set(0, (uint32_t)time(0));
    parms.set(1, nodeid);
    parms.set(2, DS::Vault::e_NodeAgeInfo);
    PGresult* result = PQexecParams(s_postgres,"UPDATE vault.\"Nodes\" SET"
                       "    \"ModifyTime\"=$1, \"Int32_2\"=1 WHERE idx=$2"
                       "     AND \"NodeType\"=$3",
                       3, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::e_NetInternalError;
    } else {
        dm_auth_bcast_node(nodeid, gen_uuid());
        PQclear(result);
        return DS::e_NetSuccess;
    }
}

uint32_t dm_auth_set_private(uint32_t nodeid)
{
    PostgresStrings<3> parms;
    parms.set(0, (uint32_t)time(0));
    parms.set(1, DS::Vault::e_NodeAgeInfo);
    parms.set(2, nodeid);
    PGresult* result = PQexecParams(s_postgres, "UPDATE vault.\"Nodes\" SET"
                       "    \"Int32_2\"=0, \"ModifyTime\"=$1"
                       "    WHERE \"NodeType\"=$2 AND idx=$3",
                       3, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                 __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::e_NetInternalError;
    } else {
        dm_auth_bcast_node(nodeid, gen_uuid());
        PQclear(result);
        return DS::e_NetSuccess;
    }
}

void dm_auth_set_pub_priv(Auth_SetPublic* msg)
{
    uint32_t result;
    if (msg->m_public)
        result = dm_auth_set_public(msg->m_node);
    else
        result = dm_auth_set_private(msg->m_node);
    SEND_REPLY(msg, result);
}

void dm_auth_createScore(Auth_CreateScore* msg)
{
    PostgresStrings<4> parms;
    parms.set(0, msg->m_owner);
    parms.set(1, msg->m_type);
    parms.set(2, msg->m_name);
    parms.set(3, msg->m_points);
    PGresult* result = PQexecParams(s_postgres,
                                    "SELECT auth.create_score($1, $2, $3, $4);",
                                    4, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    msg->m_scoreId = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);
    if (msg->m_scoreId == static_cast<uint32_t>(-1))
        SEND_REPLY(msg, DS::e_NetScoreAlreadyExists);
    else
        SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_getScores(Auth_GetScores* msg)
{
    PostgresStrings<2> parms;
    parms.set(0, msg->m_owner);
    parms.set(1, msg->m_name);
    PGresult* result = PQexecParams(s_postgres,
                                    "SELECT idx, \"CreateTime\", \"Type\", \"Points\""
                                    "    FROM auth.\"Scores\" WHERE \"OwnerIdx\"=$1 AND"
                                    "    \"Name\"=$2",
                                    2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    msg->m_scores.reserve(PQntuples(result));
    for (int i = 0; i < PQntuples(result); ++i) {
        Auth_GetScores::GameScore score;
        score.m_scoreId = strtoul(PQgetvalue(result, i, 0), 0, 10);
        score.m_owner = msg->m_owner;
        score.m_createTime = strtoul(PQgetvalue(result, i, 1), 0, 10);
        score.m_type = strtoul(PQgetvalue(result, i, 2), 0, 10);
        score.m_points = strtoul(PQgetvalue(result, i, 3), 0, 10);
        msg->m_scores.push_back(score);
    }
    PQclear(result);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_addScorePoints(Auth_UpdateScore* msg)
{
    PostgresStrings<3> parms;
    parms.set(0, msg->m_scoreId);
    PGresult* result = PQexecParams(s_postgres,
                                   "SELECT \"Type\" FROM auth.\"Scores\" WHERE idx=$1",
                                    1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) != 1) {
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetScoreNoDataFound);
        return;
    }
    uint32_t scoreType = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);
    if (scoreType == Auth_UpdateScore::e_Fixed) {
        SEND_REPLY(msg, DS::e_NetScoreWrongType);
        return;
    }

    // Passed all sanity checks, update score.
    parms.set(1, msg->m_points);
    parms.set(2, static_cast<uint32_t>(scoreType == Auth_UpdateScore::e_Golf));
    result = PQexecParams(s_postgres,
                          "SELECT auth.add_score_points($1, $2, $3);",
                          3, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
    } else {
        // the prepared statement returns a result, but the op always succeeds
        // to some degree, so let's pretend everything is hunky-dory
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetSuccess);
    }
}

void dm_auth_transferScorePoints(Auth_TransferScore* msg)
{
    PostgresStrings<4> parms;
    parms.set(0, msg->m_srcScoreId);
    parms.set(1, msg->m_dstScoreId);
    PGresult* result = PQexecParams(s_postgres,
                       "SELECT \"Type\" FROM auth.\"Scores\""
                       "    WHERE idx=$1 OR idx=$2",
                       2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    } else if (PQntuples(result) != 2) {
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetScoreNoDataFound);
        return;
    }
    uint32_t srcType = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    uint32_t dstType = strtoul(PQgetvalue(result, 1, 0), 0, 10);
    bool allowNegative = false;
    PQclear(result);
    if (srcType == Auth_UpdateScore::e_Fixed || dstType == Auth_UpdateScore::e_Fixed) {
        SEND_REPLY(msg, DS::e_NetScoreWrongType);
        return;
    }
    if (srcType == Auth_UpdateScore::e_Golf && dstType == Auth_UpdateScore::e_Golf) {
        allowNegative = true;
    }
    parms.set(2, msg->m_points);
    parms.set(3, static_cast<uint32_t>(allowNegative));
    result = PQexecParams(s_postgres,
             "SELECT auth.transfer_score_points($1, $2, $3, $4)",
             4, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    uint32_t status = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);
    SEND_REPLY(msg, (status != 0) ? DS::e_NetSuccess : DS::e_NetScoreNotEnoughPoints);
}

void dm_auth_setScorePoints(Auth_UpdateScore* msg)
{
    PostgresStrings<2> parms;
    parms.set(0, msg->m_scoreId);
    PGresult* result = PQexecParams(s_postgres,
                                   "SELECT \"Type\" FROM auth.\"Scores\" WHERE idx=$1",
                                    1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) != 1) {
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetScoreNoDataFound);
        return;
    }
    uint32_t scoreType = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);
    if (scoreType != Auth_UpdateScore::e_Fixed) {
        SEND_REPLY(msg, DS::e_NetScoreWrongType);
        return;
    }
    parms.set(1, msg->m_points);
    result = PQexecParams(s_postgres,
                          "UPDATE auth.\"Scores\" SET \"Points\"=$2 WHERE idx=$1",
                          2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    PQclear(result);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_getHighScores(Auth_GetHighScores* msg)
{
    PostgresStrings<3> parms;
    PGresult* result;
    if (msg->m_owner == 0) {
        parms.set(0, msg->m_name);
        parms.set(1, msg->m_maxScores);
        result = PQexecParams(s_postgres,
                              "SELECT idx, \"CreateTime\", \"Type\", \"Points\""
                              "    FROM auth.\"Scores\" WHERE \"Name\"=$1"
                              "    LIMIT $2",
                              2, 0, parms.m_values, 0, 0, 0);
    } else {
        parms.set(0, msg->m_owner);
        parms.set(1, DS::Vault::e_AgeOwnersFolder);
        result = PQexecParams(s_postgres,
                              "SELECT idx FROM vault.find_folder($1, $2)",
                               2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            SEND_REPLY(msg, DS::e_NetInternalError);
            return;
        }
        DS_PASSERT(PQntuples(result) == 1);
        uint32_t ageOwnersFolder = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);

        parms.set(0, msg->m_name);
        parms.set(1, ageOwnersFolder);
        parms.set(2, msg->m_maxScores);
        result = PQexecParams(s_postgres,
                              "SELECT idx, \"OwnerIdx\", \"CreateTime\", \"Type\", \"Points\""
                              "    FROM auth.\"Scores\" WHERE \"Name\"=$1"
                              "    AND \"OwnerIdx\" IN (SELECT \"ChildIdx\""
                              "    FROM vault.\"NodeRefs\" WHERE \"ParentIdx\"=$2)"
                              "    LIMIT $3",
                              3, 0, parms.m_values, 0, 0, 0);
    }
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    msg->m_scores.reserve(PQntuples(result));
    for (int i = 0; i < PQntuples(result); ++i) {
        Auth_GetScores::GameScore score;
        score.m_scoreId = strtoul(PQgetvalue(result, i, 0), 0, 10);
        score.m_owner = strtoul(PQgetvalue(result, i, 1), 0, 10);
        score.m_createTime = strtoul(PQgetvalue(result, i, 2), 0, 10);
        score.m_type = strtoul(PQgetvalue(result, i, 3), 0, 10);
        score.m_points = strtoul(PQgetvalue(result, i, 4), 0, 10);
        msg->m_scores.push_back(score);
    }
    PQclear(result);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_updateAgeSrv(Auth_UpdateAgeSrv* msg)
{
    AuthServer_Private* client = nullptr;
    s_authClientMutex.lock();
    for (auto it = s_authClients.begin(); it != s_authClients.end(); ++it) {
        if ((*it)->m_player.m_playerId == msg->m_playerId) {
            client = *it;
            break;
        }
    }
    s_authClientMutex.unlock();

    if (client) {
        client->m_ageNodeId = msg->m_ageNodeId;
        msg->m_isAdmin = (client->m_acctFlags & DS::e_AcctAdmin);
    }
    SEND_REPLY(msg, client ? DS::e_NetSuccess : DS::e_NetPlayerNotFound);
}

void dm_auth_acctFlags(Auth_AccountFlags* msg)
{
    PostgresStrings<2> parms;
    parms.set(0, msg->m_acctName);
    PGresult* result = PQexecParams(s_postgres,
                                    "SELECT \"AcctFlags\" FROM auth.\"Accounts\""
                                    "    WHERE LOWER(\"Login\")=LOWER($1)",
                                    1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (PQntuples(result) != 1) {
        fprintf(stderr, "%s:%d:\n    Expected 1 row, got %d\n",
                __FILE__, __LINE__, PQntuples(result));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }

    uint32_t acctFlags = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);

    /* Thar be moar majick */
#define TOGGLE_FLAG(flag) \
    if (msg->m_flags & flag) { \
        if (acctFlags & flag) \
            acctFlags &= ~flag; \
        else \
            acctFlags |= flag; \
    }
    TOGGLE_FLAG(DS::e_AcctAdmin);
    TOGGLE_FLAG(DS::e_AcctBanned);
    TOGGLE_FLAG(DS::e_AcctBetaTester);
#undef TOGGLE_FLAG

    if (msg->m_flags != 0) {
        parms.set(1, acctFlags);
        result = PQexecParams(s_postgres,
                              "UPDATE auth.\"Accounts\" SET \"AcctFlags\"=$2"
                              "    WHERE LOWER(\"Login\")=LOWER($1)",
                              2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            SEND_REPLY(msg, DS::e_NetInternalError);
            return;
        }
        PQclear(result);
    }
    msg->m_flags = acctFlags;
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_addAllPlayers(Auth_AddAllPlayers* msg)
{
    check_postgres();

    if (v_has_node(msg->m_playerId, s_allPlayers)) {
        if (!v_unref_node(msg->m_playerId, s_allPlayers)) {
            SEND_REPLY(msg, DS::e_NetInternalError);
            return;
        }
        dm_auth_bcast_unref({msg->m_playerId, s_allPlayers, 0, 0});
    } else {
        if (!v_ref_node(msg->m_playerId, s_allPlayers, 0)) {
            SEND_REPLY(msg, DS::e_NetInternalError);
            return;
        }
        dm_auth_bcast_ref({msg->m_playerId, s_allPlayers, 0, 0});
    }

    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_fetchSDL(Auth_FetchSDL* msg)
{
    auto global_it = s_globalStates.find(msg->m_ageFilename);
    if (global_it != s_globalStates.end()) {
        msg->m_globalState = global_it->second;
    }

    if (msg->m_sdlNodeId == 0) {
        // TODO: Determine if there actually is a non-null state and save to vault
        msg->m_localState = gen_default_sdl(msg->m_ageFilename);
    } else {
        DS::Vault::Node sdlNode = v_fetch_node(msg->m_sdlNodeId);
        msg->m_localState = sdlNode.m_Blob_1;
    }

    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_auth_update_globalSDL(Auth_UpdateGlobalSDL* msg)
{
    auto it = s_globalStates.find(msg->m_ageFilename);
    if (it == s_globalStates.end()) {
        SEND_REPLY(msg, DS::e_NetStateObjectNotFound);
        return;
    }

    SDL::State state = it->second;
    for (size_t i = 0; i < state.data()->m_simpleVars.size(); ++i) {
        SDL::Variable* var = state.data()->m_simpleVars[i];
        if (var->descriptor()->m_name == msg->m_variable) {
            var->data()->m_flags |= SDL::Variable::e_HasTimeStamp | SDL::Variable::e_XIsDirty;
            var->data()->m_timestamp.setNow();

            if (msg->m_value.is_empty()) {
                var->setDefault();
            } else {
                var->data()->m_flags &= ~SDL::Variable::e_SameAsDefault;
                switch (var->descriptor()->m_type) {
                case SDL::e_VarBool:
                    var->data()->m_bool[0] = msg->m_value.to_bool();
                    break;
                case SDL::e_VarByte:
                    var->data()->m_byte[0] = static_cast<int8_t>(msg->m_value.to_uint());
                    break;
                case SDL::e_VarInt:
                    var->data()->m_int[0] = msg->m_value.to_int();
                    break;
                case SDL::e_VarShort:
                    var->data()->m_short[0] = static_cast<int16_t>(msg->m_value.to_int());
                    break;
                case SDL::e_VarString:
                    var->data()->m_string[0] = msg->m_value;
                    break;
                default:
                    SEND_REPLY(msg, DS::e_NetNotSupported);
                    return;
                }
            }

            // I guess this is a good time to save it back to the DB?
            DS::Blob blob = state.toBlob();
            PostgresStrings<2> parms;
            parms.set(0, msg->m_ageFilename);
            parms.set(1, ST::base64_encode(blob.buffer(), blob.size()));
            PGresult* result = PQexecParams(s_postgres, "UPDATE vault.\"GlobalStates\""
                                            "    SET \"SdlBlob\" = $2"
                                            "    WHERE \"Descriptor\" = $1",
                                            2, 0, parms.m_values, 0, 0, 0);
            if (PQresultStatus(result) != PGRES_COMMAND_OK) {
                fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                        __FILE__, __LINE__, PQerrorMessage(s_postgres));
                // This doesn't block continuing...
                DS_DASSERT(false);
            }

            DS::GameServer_UpdateGlobalSDL(msg->m_ageFilename);
            SEND_REPLY(msg, DS::e_NetSuccess);
            return;
        }
    }

    // If we got here, then we didn't find a variable...
    SEND_REPLY(msg, DS::e_NetInvalidParameter);
}

void dm_authDaemon()
{
    s_postgres = PQconnectdb(ST::format(
                    "host='{}' port='{}' user='{}' password='{}' dbname='{}'",
                    DS::Settings::DbHostname(), DS::Settings::DbPort(),
                    DS::Settings::DbUsername(), DS::Settings::DbPassword(),
                    DS::Settings::DbDbaseName()).c_str());
    if (PQstatus(s_postgres) != CONNECTION_OK) {
        fprintf(stderr, "Error connecting to postgres: %s", PQerrorMessage(s_postgres));
        PQfinish(s_postgres);
        s_postgres = 0;
        return;
    }

    if (!dm_vault_init()) {
        fputs("[Auth] Vault failed to initialize\n", stderr);
        return;
    }
    if (!dm_global_sdl_init()) {
        fputs("[Auth] AllAgeGlobalSDL failed to initialize\n", stderr);
        return;
    }
    if (!dm_check_static_ages()) {
        fputs("[Auth] Failed to initialize static ages\n", stderr);
        return;
    }
    if (!dm_all_players_init()) {
        fputs("[Auth] AllPlayers folder failed to initialize\n", stderr);
        return;
    }

    // Mark all player info nodes offline
    PostgresStrings<1> parms;
    parms.set(0, DS::Vault::e_NodePlayerInfo);
    PGresult* result = PQexecParams(s_postgres,
                                    "UPDATE vault.\"Nodes\" SET"
                                    "    \"Int32_1\" = 0"
                                    "    WHERE \"NodeType\" = $1",
                                    1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                 __FILE__, __LINE__, PQerrorMessage(s_postgres));
        // This doesn't block continuing...
        DS_DASSERT(false);
    }

    for ( ;; ) {
        DS::FifoMessage msg = s_authChannel.getMessage();
        try {
            switch (msg.m_messageType) {
            case e_AuthShutdown:
                dm_auth_shutdown();
                return;
            case e_AuthClientLogin:
                dm_auth_login(reinterpret_cast<Auth_LoginInfo*>(msg.m_payload));
                break;
            case e_AuthSetPlayer:
                dm_auth_setPlayer(reinterpret_cast<Auth_ClientMessage*>(msg.m_payload));
                break;
            case e_AuthCreatePlayer:
                dm_auth_createPlayer(reinterpret_cast<Auth_PlayerCreate*>(msg.m_payload));
                break;
            case e_AuthDeletePlayer:
                dm_auth_deletePlayer(reinterpret_cast<Auth_PlayerDelete*>(msg.m_payload));
                break;
            case e_VaultCreateNode:
                {
                    Auth_NodeInfo* info = reinterpret_cast<Auth_NodeInfo*>(msg.m_payload);
                    uint32_t nodeIdx = v_create_node(info->m_node);
                    if (nodeIdx != 0) {
                        info->m_node.set_NodeIdx(nodeIdx);
                        SEND_REPLY(info, DS::e_NetSuccess);
                    } else {
                        SEND_REPLY(info, DS::e_NetInternalError);
                    }
                }
                break;
            case e_VaultFetchNode:
                {
                    Auth_NodeInfo* info = reinterpret_cast<Auth_NodeInfo*>(msg.m_payload);
                    info->m_node = v_fetch_node(info->m_node.m_NodeIdx);
                    if (info->m_node.isNull())
                        SEND_REPLY(info, DS::e_NetVaultNodeNotFound);
                    else
                        SEND_REPLY(info, DS::e_NetSuccess);
                }
                break;
            case e_VaultUpdateNode:
                {
                    Auth_NodeInfo* info = reinterpret_cast<Auth_NodeInfo*>(msg.m_payload);
                    if (!info->m_internal && info->m_node.m_NodeType == DS::Vault::e_NodeSDL) {
                        // This is an SDL update. It needs to be passed off to the gameserver
                        PostgresStrings<1> parms;
                        parms.set(0, info->m_node.m_NodeIdx);
                        PGresult* result = PQexecParams(s_postgres,
                                                        "SELECT \"idx\" FROM game.\"Servers\" WHERE \"SdlIdx\"=$1",
                                                        1, 0, parms.m_values, 0, 0, 0);
                        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
                            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
                            PQclear(result);
                            SEND_REPLY(info, DS::e_NetInternalError);
                            break;
                        }
                        if (PQntuples(result) != 0) {
                            uint32_t ageMcpId = strtoul(PQgetvalue(result, 0, 0), 0, 10);
                            PQclear(result);
                            if (DS::GameServer_UpdateVaultSDL(info->m_node, ageMcpId)) {
                                SEND_REPLY(info, DS::e_NetSuccess);
                                break;
                            }
                        }
                    }
                    if (info->m_revision.isNull()) {
                        info->m_revision = gen_uuid();
                    }
                    if (v_update_node(info->m_node)) {
                        // Broadcast the change
                        dm_auth_bcast_node(info->m_node.m_NodeIdx, info->m_revision);
                        SEND_REPLY(info, DS::e_NetSuccess);
                    } else {
                        SEND_REPLY(info, DS::e_NetInternalError);
                    }
                }
                break;
            case e_VaultRefNode:
                {
                    Auth_NodeRef* info = reinterpret_cast<Auth_NodeRef*>(msg.m_payload);
                    if (v_ref_node(info->m_ref.m_parent, info->m_ref.m_child, info->m_ref.m_owner)) {
                        // Broadcast the change
                        dm_auth_bcast_ref(info->m_ref);
                        SEND_REPLY(info, DS::e_NetSuccess);
                    } else {
                        SEND_REPLY(info, DS::e_NetInternalError);
                    }
                }
                break;
            case e_VaultSendNode:
                {
                    Auth_NodeSend* info = reinterpret_cast<Auth_NodeSend*>(msg.m_payload);
                    DS::Vault::NodeRef ref = v_send_node(info->m_nodeIdx, info->m_playerIdx, info->m_senderIdx);
                    if (ref.m_child || ref.m_owner || ref.m_parent)
                        dm_auth_bcast_ref(ref);
                    // There's no way to indicate success or failure to the client. Whether or not it gets a NodeRef
                    // message is the only way the client knows if all went well here.
                    // This reply is purely for synchronization purposes.
                    SEND_REPLY(info, 0);
                }
                break;
            case e_VaultUnrefNode:
                {
                    Auth_NodeRef* info = reinterpret_cast<Auth_NodeRef*>(msg.m_payload);
                    if (v_unref_node(info->m_ref.m_parent, info->m_ref.m_child)) {
                        // Broadcast the change
                        dm_auth_bcast_unref(info->m_ref);
                        SEND_REPLY(info, DS::e_NetSuccess);
                    } else {
                        SEND_REPLY(info, DS::e_NetInternalError);
                    }
                }
                break;
            case e_VaultFetchNodeTree:
                {
                    Auth_NodeRefList* info = reinterpret_cast<Auth_NodeRefList*>(msg.m_payload);
                    if (v_fetch_tree(info->m_nodeId, info->m_refs))
                        SEND_REPLY(info, DS::e_NetSuccess);
                    else
                        SEND_REPLY(info, DS::e_NetInternalError);
                }
                break;
            case e_VaultFindNode:
                {
                    Auth_NodeFindList* info = reinterpret_cast<Auth_NodeFindList*>(msg.m_payload);
                    if (v_find_nodes(info->m_template, info->m_nodes))
                        SEND_REPLY(info, DS::e_NetSuccess);
                    else
                        SEND_REPLY(info, DS::e_NetInternalError);
                }
                break;
            case e_VaultInitAge:
                dm_auth_createAge(reinterpret_cast<Auth_AgeCreate*>(msg.m_payload));
                break;
            case e_AuthFindGameServer:
                dm_auth_findAge(reinterpret_cast<Auth_GameAge*>(msg.m_payload));
                break;
            case e_AuthDisconnect:
                dm_auth_disconnect(reinterpret_cast<Auth_ClientMessage*>(msg.m_payload));
                break;
            case e_AuthAddAcct:
                dm_auth_addacct(reinterpret_cast<Auth_AddAcct*>(msg.m_payload));
                break;
            case e_AuthGetPublic:
                dm_auth_get_public(reinterpret_cast<Auth_PubAgeRequest*>(msg.m_payload));
                break;
            case e_AuthSetPublic:
                dm_auth_set_pub_priv(reinterpret_cast<Auth_SetPublic*>(msg.m_payload));
                break;
            case e_AuthCreateScore:
                dm_auth_createScore(reinterpret_cast<Auth_CreateScore*>(msg.m_payload));
                break;
            case e_AuthGetScores:
                dm_auth_getScores(reinterpret_cast<Auth_GetScores*>(msg.m_payload));
                break;
            case e_AuthAddScorePoints:
                dm_auth_addScorePoints(reinterpret_cast<Auth_UpdateScore*>(msg.m_payload));
                break;
            case e_AuthTransferScorePoints:
                dm_auth_transferScorePoints(reinterpret_cast<Auth_TransferScore*>(msg.m_payload));
                break;
            case e_AuthSetScorePoints:
                dm_auth_setScorePoints(reinterpret_cast<Auth_UpdateScore*>(msg.m_payload));
                break;
            case e_AuthGetHighScores:
                dm_auth_getHighScores(reinterpret_cast<Auth_GetHighScores*>(msg.m_payload));
                break;
            case e_AuthUpdateAgeSrv:
                dm_auth_updateAgeSrv(reinterpret_cast<Auth_UpdateAgeSrv*>(msg.m_payload));
                break;
            case e_AuthAcctFlags:
                dm_auth_acctFlags(reinterpret_cast<Auth_AccountFlags*>(msg.m_payload));
                break;
            case e_AuthRestrictLogins:
                s_restrictLogins = !s_restrictLogins;
                if (msg.m_payload) {
                    Auth_RestrictLogins* info = reinterpret_cast<Auth_RestrictLogins*>(msg.m_payload);
                    info->m_status = s_restrictLogins;
                    SEND_REPLY(info, DS::e_NetSuccess);
                }
                break;
            case e_AuthAddAllPlayers:
                dm_auth_addAllPlayers(reinterpret_cast<Auth_AddAllPlayers*>(msg.m_payload));
                break;
            case e_AuthFetchSDL:
                dm_auth_fetchSDL(reinterpret_cast<Auth_FetchSDL*>(msg.m_payload));
                break;
            case e_AuthUpdateGlobalSDL:
                dm_auth_update_globalSDL(reinterpret_cast<Auth_UpdateGlobalSDL*>(msg.m_payload));
                break;
            default:
                /* Invalid message...  This shouldn't happen */
                DS_DASSERT(0);
                break;
            }
        } catch (const DS::AssertException& ex) {
            fprintf(stderr, "[Auth] Assertion failed at %s:%ld:  %s\n",
                    ex.m_file, ex.m_line, ex.m_cond);
            if (msg.m_payload) {
                // Keep clients from blocking on a reply
                SEND_REPLY(reinterpret_cast<Auth_ClientMessage*>(msg.m_payload),
                           DS::e_NetInternalError);
            }
        } catch (const DS::PacketSizeOutOfBounds& ex) {
            fprintf(stderr, "[Auth] Client packet size too large: Requested %u bytes\n",
                    ex.requestedSize());
        }
    }

    dm_auth_shutdown();
}
