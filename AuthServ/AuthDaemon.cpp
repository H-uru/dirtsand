/******************************************************************************
 * This file is part of dirtsand.                                             *
 *                                                                            *
 * dirtsand is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation, either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * dirtsand is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.          *
 ******************************************************************************/

#include "AuthServer_Private.h"
#include "GameServ/GameServer.h"
#include "encodings.h"
#include "settings.h"
#include "errors.h"
#include <chrono>

std::thread s_authDaemonThread;
DS::MsgChannel s_authChannel;
PGconn* s_postgres;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

static inline void check_postgres()
{
    if (PQstatus(s_postgres) == CONNECTION_BAD)
        PQreset(s_postgres);
    DS_DASSERT(PQstatus(s_postgres) == CONNECTION_OK);
}

void dm_auth_addacct(Auth_AccountInfo* info)
{
    DS::StringBuffer<chr8_t> pwBuf = info->m_password.toUtf8();
    DS::ShaHash pwHash = DS::ShaHash::Sha1(pwBuf.data(), pwBuf.length());
    PostgresStrings<3> iparms;
    iparms.set(0, gen_uuid().toString());
    iparms.set(1, pwHash.toString());
    iparms.set(2, info->m_acctName);
    PGresult* result = PQexecParams(s_postgres,
            "INSERT INTO auth.\"Accounts\""
            "    (\"AcctUuid\", \"PassHash\", \"Login\", \"AcctFlags\", \"BillingType\")"
            "    VALUES ($1, $2, $3, 0, 1)"
            "    RETURNING idx",
            3, 0, iparms.m_values, 0, 0, 0);
    delete info;
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return;
    }
    DS_DASSERT(PQntuples(result) == 1);
    PQclear(result);
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
        fprintf(stderr, "[Auth] Clients didn't die after 5 seconds!\n");

    PQfinish(s_postgres);
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
    info->m_acctFlags = strtoul(PQgetvalue(result, 0, 2), 0, 10);
    info->m_billingType = strtoul(PQgetvalue(result, 0, 3), 0, 10);
    printf("[Auth] %s logged in as %s {%s}\n",
           DS::SockIpAddress(info->m_client->m_sock).c_str(),
           info->m_acctName.c_str(), client->m_acctUuid.toString().c_str());
    PQclear(result);

    // Get list of players
    DS::String uuidString = client->m_acctUuid.toString();
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
    uint8_t buffer[22];  // Msg ID, Node ID, Revision Uuid
    *reinterpret_cast<uint16_t*>(buffer    ) = e_AuthToCli_VaultNodeChanged;
    *reinterpret_cast<uint32_t*>(buffer + 2) = nodeIdx;
    *reinterpret_cast<DS::Uuid*>(buffer + 6) = revision;

    std::lock_guard<std::mutex> authClientGuard(s_authClientMutex);
    for (auto client_iter = s_authClients.begin(); client_iter != s_authClients.end(); ++client_iter) {
        try {
            DS::CryptSendBuffer((*client_iter)->m_sock, (*client_iter)->m_crypt, buffer, 22);
        } catch (DS::SockHup) {
            // Client ignored us.  Return the favor
        }
    }
}

void dm_auth_bcast_ref(const DS::Vault::NodeRef& ref)
{
    uint8_t buffer[14];  // Msg ID, Parent, Child, Owner
    *reinterpret_cast<uint16_t*>(buffer     ) = e_AuthToCli_VaultNodeAdded;
    *reinterpret_cast<uint32_t*>(buffer +  2) = ref.m_parent;
    *reinterpret_cast<uint32_t*>(buffer +  6) = ref.m_child;
    *reinterpret_cast<uint32_t*>(buffer + 10) = ref.m_owner;

    std::lock_guard<std::mutex> authClientGuard(s_authClientMutex);
    for (auto client_iter = s_authClients.begin(); client_iter != s_authClients.end(); ++client_iter) {
        try {
            DS::CryptSendBuffer((*client_iter)->m_sock, (*client_iter)->m_crypt, buffer, 14);
        } catch (DS::SockHup) {
            // Client ignored us.  Return the favor
        }
    }
}

void dm_auth_bcast_unref(const DS::Vault::NodeRef& ref)
{
    uint8_t buffer[10];  // Msg ID, Parent, Child
    *reinterpret_cast<uint16_t*>(buffer    ) = e_AuthToCli_VaultNodeRemoved;
    *reinterpret_cast<uint32_t*>(buffer + 2) = ref.m_parent;
    *reinterpret_cast<uint32_t*>(buffer + 6) = ref.m_child;

    std::lock_guard<std::mutex> authClientGuard(s_authClientMutex);
    for (auto client_iter = s_authClients.begin(); client_iter != s_authClients.end(); ++client_iter) {
        try {
            DS::CryptSendBuffer((*client_iter)->m_sock, (*client_iter)->m_crypt, buffer, 10);
        } catch (DS::SockHup) {
            // Client ignored us.  Return the favor
        }
    }
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
    msg->m_player.m_playerId = std::get<0>(v_create_player(client->m_acctUuid, msg->m_player));
    if (msg->m_player.m_playerId == 0)
        SEND_REPLY(msg, DS::e_NetInternalError);

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
    DS::String ageDesc;
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
    PostgresStrings<1> parms;
    parms.set(0, msg->m_agename);
    PGresult* result = PQexecParams(s_postgres,
                                    "SELECT idx, \"AgeUuid\", \"AgeInstName\", \"AgeUserName\", \"AgeDesc\", \"SeqNumber\", \"Language\", \"CurrentPopulation\", \"Population\" FROM game.\"PublicAges\""
                                    "    WHERE \"AgeFilename\"=$1",
                                    1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    for (int i = 0; i < PQntuples(result); i++) {
        Auth_PubAgeRequest::NetAgeInfo ai;
        ai.m_instance = DS::Uuid(PQgetvalue(result, i, 1));
        ai.m_instancename = PQgetvalue(result, i, 2);
        ai.m_username = PQgetvalue(result, i, 3);
        ai.m_description = PQgetvalue(result, i, 4);
        ai.m_sequence = strtoul(PQgetvalue(result, i, 5), 0, 10);
        ai.m_language = strtoul(PQgetvalue(result, i, 6), 0, 10);
        ai.m_curPopulation = strtoul(PQgetvalue(result, i, 7), 0, 10);
        ai.m_population = strtoul(PQgetvalue(result, i, 8), 0, 10);
        msg->m_ages.push_back(ai);
    }
    PQclear(result);
    SEND_REPLY(msg, DS::e_NetSuccess);
}

uint32_t dm_auth_set_public(uint32_t nodeid)
{
    PostgresStrings<8> parms;
    parms.set(0, nodeid);
    parms.set(1, DS::Vault::e_NodeAgeInfo);
    PGresult* result = PQexecParams(s_postgres,
                                    "SELECT \"Uuid_1\", \"String64_2\", \"String64_3\", \"String64_4\", \"Text_1\", \"Int32_1\", \"Int32_3\" FROM vault.\"Nodes\""
                                    "    WHERE idx=$1 AND \"NodeType\"=$2",
                                    2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::e_NetInternalError;
    }
    if (PQntuples(result) == 0) {
        // the requested age wasn't public, so we throw an error
        PQclear(result);
        return DS::e_NetInvalidParameter;
    }
    parms.set(0, PQgetvalue(result, 0, 0));
    parms.set(1, PQgetvalue(result, 0, 1));
    parms.set(2, PQgetvalue(result, 0, 2));
    parms.set(3, PQgetvalue(result, 0, 3));
    parms.set(4, PQgetvalue(result, 0, 4));
    parms.set(5, PQgetvalue(result, 0, 5));
    parms.set(6, PQgetvalue(result, 0, 6));
    parms.set(7, DS::GameServer_GetNumClients(DS::Uuid(PQgetvalue(result, 0, 0))));

    PQclear(result);
    result = PQexecParams(s_postgres,
                          "INSERT INTO game.\"PublicAges\" (\"AgeUuid\", \"AgeFilename\", \"AgeInstName\", \"AgeUserName\", \"AgeDesc\", \"SeqNumber\", \"Language\", \"CurrentPopulation\", \"Population\")"
                          "    VALUES ( $1, $2, $3, $4, $5, $6, $7, $8, 0 )",
                          8, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::e_NetInternalError;
    }

    parms.set(0, DS::Vault::e_NodeAgeInfo);
    parms.set(1, nodeid);
    result = PQexecParams(s_postgres,
                          "UPDATE vault.\"Nodes\" SET"
                          "    \"Int32_2\"=1"
                          "    WHERE \"NodeType\"=$1 AND idx=$2",
                          2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
      // This doesn't block continuing...
    } else {
        dm_auth_bcast_node(nodeid, gen_uuid());
    }

    return DS::e_NetSuccess;
}

uint32_t dm_auth_set_private(uint32_t nodeid)
{
    PostgresStrings<4> parms;
    parms.set(0, nodeid);
    parms.set(1, DS::Vault::e_NodeAgeInfo);
    PGresult* result = PQexecParams(s_postgres,
                                    "SELECT \"Uuid_1\" FROM vault.\"Nodes\" WHERE idx=$1 AND \"NodeType\"=$2",
                                    2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::e_NetInternalError;
    }
    if (PQntuples(result) == 0) {
        fprintf(stderr, "%s:%d:\n    No such ageinfo node: %d\n",
                __FILE__, __LINE__, nodeid);
        // the requested age doesn't exist
        PQclear(result);
        return DS::e_NetInvalidParameter;
    }

    parms.set(0, PQgetvalue(result, 0, 0));
    result = PQexecParams(s_postgres,
                          "DELETE FROM game.\"PublicAges\" WHERE \"AgeUuid\"=$1",
                          1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres DELETE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::e_NetInternalError;
    }

    parms.set(0, DS::Vault::e_NodeAgeInfo);
    parms.set(1, nodeid);
    result = PQexecParams(s_postgres,
                          "UPDATE vault.\"Nodes\" SET"
                          "    \"Int32_2\"=0"
                          "    WHERE \"NodeType\"=$1 AND idx=$2",
                          2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                 __FILE__, __LINE__, PQerrorMessage(s_postgres));
         // This doesn't block continuing...
    } else {
        dm_auth_bcast_node(nodeid, gen_uuid());
    }

    return DS::e_NetSuccess;
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

void dm_authDaemon()
{
    s_postgres = PQconnectdb(DS::String::Format(
                    "host='%s' port='%s' user='%s' password='%s' dbname='%s'",
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
        fprintf(stderr, "[Auth] Vault failed to initialize\n");
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

    // Mark all public ages as having a current population of ZERO
    result = PQexec(s_postgres,
                              "UPDATE game.\"PublicAges\" SET"
                              "    \"CurrentPopulation\" = 0");
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        // This doesn't block continuing...
        PQclear(result);
        DS_DASSERT(false);
    } else
        PQclear(result);

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
                    if (!info->m_revision.isNull() && info->m_node.m_NodeType == DS::Vault::e_NodeSDL) {
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
                dm_auth_addacct(reinterpret_cast<Auth_AccountInfo*>(msg.m_payload));
                break;
            case e_AuthGetPublic:
                dm_auth_get_public(reinterpret_cast<Auth_PubAgeRequest*>(msg.m_payload));
                break;
            case e_AuthSetPublic:
                dm_auth_set_pub_priv(reinterpret_cast<Auth_SetPublic*>(msg.m_payload));
                break;
            default:
                /* Invalid message...  This shouldn't happen */
                DS_DASSERT(0);
                break;
            }
        } catch (DS::AssertException ex) {
            fprintf(stderr, "[Auth] Assertion failed at %s:%ld:  %s\n",
                    ex.m_file, ex.m_line, ex.m_cond);
            if (msg.m_payload) {
                // Keep clients from blocking on a reply
                SEND_REPLY(reinterpret_cast<Auth_ClientMessage*>(msg.m_payload),
                           DS::e_NetInternalError);
            }
        }
    }

    dm_auth_shutdown();
}
