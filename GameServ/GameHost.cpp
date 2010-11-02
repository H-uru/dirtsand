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

#include "GameServer_Private.h"
#include "PlasMOUL/NetMessages/NetMsgMembersList.h"
#include "PlasMOUL/NetMessages/NetMsgLoadClone.h"
#include "PlasMOUL/NetMessages/NetMsgGameState.h"
#include "PlasMOUL/NetMessages/NetMsgSharedState.h"
#include "PlasMOUL/NetMessages/NetMsgSDLState.h"
#include "PlasMOUL/NetMessages/NetMsgGroupOwner.h"
#include "PlasMOUL/Messages/ServerReplyMsg.h"
#include "settings.h"
#include "errors.h"
#include "encodings.h"

hostmap_t s_gameHosts;
pthread_mutex_t s_gameHostMutex;
agemap_t s_ages;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

#define DM_WRITEMSG(host, msg) \
    host->m_buffer.truncate(); \
    host->m_buffer.write<uint16_t>(e_GameToCli_PropagateBuffer); \
    host->m_buffer.write<uint32_t>(msg->type()); \
    host->m_buffer.write<uint32_t>(0); \
    MOUL::Factory::WriteCreatable(&host->m_buffer, msg); \
    host->m_buffer.seek(6, SEEK_SET); \
    host->m_buffer.write<uint32_t>(host->m_buffer.size() - 10)

static inline void check_postgres(GameHost_Private* host)
{
    if (PQstatus(host->m_postgres) == CONNECTION_BAD)
        PQreset(host->m_postgres);
    DS_DASSERT(PQstatus(host->m_postgres) == CONNECTION_OK);
}

void dm_game_shutdown(GameHost_Private* host)
{
    pthread_mutex_lock(&host->m_clientMutex);
    std::tr1::unordered_map<uint32_t, GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter)
        DS::CloseSock(client_iter->second->m_sock);
    pthread_mutex_unlock(&host->m_clientMutex);

    bool complete = false;
    for (int i=0; i<50 && !complete; ++i) {
        pthread_mutex_lock(&host->m_clientMutex);
        size_t alive = host->m_clients.size();
        pthread_mutex_unlock(&host->m_clientMutex);
        if (alive == 0)
            complete = true;
        usleep(100000);
    }
    if (!complete)
        fprintf(stderr, "[Game] Clients didn't die after 5 seconds!\n");

    pthread_mutex_lock(&s_gameHostMutex);
    hostmap_t::iterator host_iter = s_gameHosts.begin();
    while (host_iter != s_gameHosts.end()) {
        if (host_iter->second == host)
            host_iter = s_gameHosts.erase(host_iter);
        else
            ++host_iter;
    }
    pthread_mutex_unlock(&s_gameHostMutex);

    pthread_mutex_destroy(&host->m_clientMutex);
    PQfinish(host->m_postgres);
    delete host;
}

void dm_propagate(GameHost_Private* host, MOUL::NetMessage* msg, uint32_t sender)
{
    DM_WRITEMSG(host, msg);

    pthread_mutex_lock(&host->m_clientMutex);
    std::tr1::unordered_map<uint32_t, GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if (client_iter->second->m_clientInfo.m_PlayerId == sender)
            continue;
        try {
            DS::CryptSendBuffer(client_iter->second->m_sock, client_iter->second->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
        } catch (DS::SockHup) {
            // This is handled below too, but we don't want to skip the rest
            // of the client list if one hung up
        }
    }
    pthread_mutex_unlock(&host->m_clientMutex);
}

void dm_propagate_to(GameHost_Private* host, MOUL::NetMessage* msg,
                     const std::vector<uint32_t>& receivers)
{
    DM_WRITEMSG(host, msg);

    pthread_mutex_lock(&host->m_clientMutex);
    std::vector<uint32_t>::const_iterator rcvr_iter;
    for (rcvr_iter = receivers.begin(); rcvr_iter != receivers.end(); ++rcvr_iter) {
        std::tr1::unordered_map<uint32_t, GameClient_Private*>::iterator client = host->m_clients.find(*rcvr_iter);
        if (client != host->m_clients.end()) {
            try {
                DS::CryptSendBuffer(client->second->m_sock, client->second->m_crypt,
                                    host->m_buffer.buffer(), host->m_buffer.size());
            } catch (DS::SockHup) {
                // This is handled below too, but we don't want to skip the rest
                // of the client list if one hung up
            }
        }
    }
    pthread_mutex_unlock(&host->m_clientMutex);
}

void dm_game_cleanup(GameHost_Private* host)
{
    // Good time to write this back to the vault
    Auth_NodeInfo sdlNode;
    AuthClient_Private fakeClient;
    sdlNode.m_client = &fakeClient;
    sdlNode.m_node.set_NodeIdx(host->m_sdlIdx);
    sdlNode.m_node.set_Blob_1(host->m_ageSdl);
    s_authChannel.putMessage(e_VaultUpdateNode, reinterpret_cast<void*>(&sdlNode));
    if (fakeClient.m_channel.getMessage().m_messageType != DS::e_NetSuccess)
        fprintf(stderr, "[Game] Error writing SDL node back to vault\n");

    //TODO: shutdown this host if no clients connect within a time limit
}

void dm_game_join(GameHost_Private* host, Game_ClientMessage* msg)
{
    MOUL::NetMsgGroupOwner* groupMsg = MOUL::NetMsgGroupOwner::Create();
    groupMsg->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                             | MOUL::NetMessage::e_IsSystemMessage
                             | MOUL::NetMessage::e_NeedsReliableSend;
    groupMsg->m_timestamp.setNow();
    groupMsg->m_groups.resize(1);
    groupMsg->m_groups[0].m_own = true;

    DM_WRITEMSG(host, groupMsg);
    DS::CryptSendBuffer(msg->m_client->m_sock, msg->m_client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
    groupMsg->unref();

    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_send_state(GameHost_Private* host, GameClient_Private* client)
{
    check_postgres(host);

    MOUL::NetMsgSDLState* state = MOUL::NetMsgSDLState::Create();
    state->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                        | MOUL::NetMessage::e_NeedsReliableSend;
    state->m_timestamp.setNow();
    state->m_isInitial = true;
    state->m_persistOnServer = true;
    state->m_isAvatar = false;

    uint32_t states = 0;
    if (host->m_ageSdl.size()) {
        Game_AgeInfo info = s_ages[host->m_ageFilename];
        state->m_object.m_location = MOUL::Location::Make(info.m_seqPrefix, -2, MOUL::Location::e_BuiltIn);
        state->m_object.m_name = "AgeSDLHook";
        state->m_object.m_type = 1;  // SceneObject
        state->m_object.m_id = 1;
        state->m_sdlBlob = host->m_ageSdl;
        DM_WRITEMSG(host, state);
        DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                            host->m_buffer.buffer(), host->m_buffer.size());
        ++states;
    }

    PostgresStrings<1> parms;
    parms.set(0, host->m_serverIdx);
    PGresult* result = PQexecParams(host->m_postgres,
            "SELECT \"ObjectKey\", \"SdlBlob\""
            "    FROM game.\"AgeStates\" WHERE \"ServerIdx\"=$1",
            1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
    } else {
        int count = PQntuples(result);

        for (int i=0; i<count; ++i) {
            DS::BlobStream bsObject(DS::Base64Decode(PQgetvalue(result, i, 0)));
            state->m_object.read(&bsObject);

            state->m_sdlBlob = DS::Base64Decode(PQgetvalue(result, i, 1));
            DM_WRITEMSG(host, state);
            DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
            ++states;
        }
    }
    PQclear(result);
    state->unref();

    // Final message indicating the whole state was sent
    MOUL::NetMsgInitialAgeStateSent* reply = MOUL::NetMsgInitialAgeStateSent::Create();
    reply->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                          | MOUL::NetMessage::e_IsSystemMessage
                          | MOUL::NetMessage::e_NeedsReliableSend;
    reply->m_timestamp.setNow();
    reply->m_numStates = states;

    DM_WRITEMSG(host, reply);
    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
    reply->unref();
}

void dm_read_sdl(GameHost_Private* host, GameClient_Private* client,
                 MOUL::NetMsgSDLState* state, bool bcast)
{
    if (state->m_object.m_name == "AgeSDLHook") {
        host->m_ageSdl = state->m_sdlBlob;
    } else if (state->m_persistOnServer) {
        check_postgres(host);

        PostgresStrings<3> parms;
        host->m_buffer.truncate();
        state->m_object.write(&host->m_buffer);
        parms.set(0, host->m_serverIdx);
        parms.set(1, DS::Base64Encode(host->m_buffer.buffer(), host->m_buffer.size()));
        parms.set(2, DS::Base64Encode(state->m_sdlBlob.buffer(), state->m_sdlBlob.size()));
        PGresult* result = PQexecParams(host->m_postgres,
            "SELECT idx FROM game.\"AgeStates\""
            "    WHERE \"ServerIdx\"=$1 AND \"ObjectKey\"=$2",
            2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
            PQclear(result);
            return;
        }
        if (PQntuples(result) == 0) {
            PQclear(result);
            result = PQexecParams(host->m_postgres,
                "INSERT INTO game.\"AgeStates\""
                "    (\"ServerIdx\", \"ObjectKey\", \"SdlBlob\")"
                "    VALUES ($1, $2, $3)",
                3, 0, parms.m_values, 0, 0, 0);
            if (PQresultStatus(result) != PGRES_COMMAND_OK) {
                fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                        __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
                PQclear(result);
                return;
            }
            PQclear(result);
        } else {
            DS_DASSERT(PQntuples(result) == 1);
            parms.set(0, DS::String(PQgetvalue(result, 0, 0)));
            PQclear(result);
            result = PQexecParams(host->m_postgres,
                "UPDATE game.\"AgeStates\""
                "    SET \"ObjectKey\"=$2, \"SdlBlob\"=$3"
                "    WHERE idx=$1",
                3, 0, parms.m_values, 0, 0, 0);
            if (PQresultStatus(result) != PGRES_COMMAND_OK) {
                fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                        __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
                PQclear(result);
                return;
            }
            PQclear(result);
        }
    }

    if (bcast) {
        MOUL::NetMsgSDLState* bcastState = MOUL::NetMsgSDLState::Create();
        bcastState->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                                   | MOUL::NetMessage::e_NeedsReliableSend;
        bcastState->m_timestamp.setNow();
        bcastState->m_isInitial = false;
        bcastState->m_persistOnServer = state->m_persistOnServer;
        bcastState->m_isAvatar = state->m_isAvatar;
        bcastState->m_object = state->m_object;
        bcastState->m_sdlBlob = state->m_sdlBlob;
        dm_propagate(host, bcastState, client->m_clientInfo.m_PlayerId);
        bcastState->unref();
    }
}

void dm_test_and_set(GameHost_Private* host, GameClient_Private* client,
                     MOUL::NetMsgTestAndSet* msg)
{
    //TODO: Whatever the client is expecting us to do

    MOUL::ServerReplyMsg* reply = MOUL::ServerReplyMsg::Create();
    reply->m_receivers.push_back(msg->m_object);
    reply->m_bcastFlags = MOUL::Message::e_LocalPropagate;
    reply->m_reply = MOUL::ServerReplyMsg::e_Affirm;

    MOUL::NetMsgGameMessage* netReply = MOUL::NetMsgGameMessage::Create();
    netReply->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                             | MOUL::NetMessage::e_NeedsReliableSend;
    netReply->m_timestamp.setNow();
    netReply->m_message = reply;

    DM_WRITEMSG(host, netReply);
    netReply->unref();
    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
}

void dm_send_members(GameHost_Private* host, GameClient_Private* client)
{
    MOUL::NetMsgMembersList* members = MOUL::NetMsgMembersList::Create();
    members->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                            | MOUL::NetMessage::e_HasPlayerID
                            | MOUL::NetMessage::e_IsSystemMessage
                            | MOUL::NetMessage::e_NeedsReliableSend;
    members->m_timestamp.setNow();
    members->m_playerId = client->m_clientInfo.m_PlayerId;

    pthread_mutex_lock(&host->m_clientMutex);
    members->m_members.reserve(host->m_clients.size() - 1);
    std::tr1::unordered_map<uint32_t, GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if (client_iter->second->m_clientInfo.m_PlayerId != client->m_clientInfo.m_PlayerId
            && !client->m_clientKey.isNull()) {
            MOUL::NetMsgMemberInfo info;
            info.m_client = client_iter->second->m_clientInfo;
            info.m_avatarKey = client_iter->second->m_clientKey;
            members->m_members.push_back(info);
        }
    }
    pthread_mutex_unlock(&host->m_clientMutex);

    DM_WRITEMSG(host, members);
    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
    members->unref();
}

void dm_game_message(GameHost_Private* host, Game_PropagateMessage* msg)
{
    DS::BlobStream stream(msg->m_message);
    MOUL::NetMessage* netmsg = 0;
    try {
        netmsg = MOUL::Factory::Read<MOUL::NetMessage>(&stream);
    } catch (MOUL::FactoryException) {
        fprintf(stderr, "[Game] Warning: Ignoring message: %04X\n",
                msg->m_messageType);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[Game] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
#ifdef DEBUG
    if (!stream.atEof()) {
        fprintf(stderr, "[Game] Warning: Incomplete parse of %04X\n",
                netmsg->type());
        netmsg->unref();
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
#endif

    try {
        switch (msg->m_messageType) {
        case MOUL::ID_NetMsgPagingRoom:
            dm_propagate(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
            break;
        case MOUL::ID_NetMsgGameStateRequest:
            dm_send_state(host, msg->m_client);
            break;
        case MOUL::ID_NetMsgGameMessage:
            if (netmsg->Cast<MOUL::NetMsgGameMessage>()->m_message->makeSafeForNet())
                dm_propagate(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
            break;
        case MOUL::ID_NetMsgGameMessageDirected:
            {
                MOUL::NetMsgGameMessageDirected* directedMsg =
                        netmsg->Cast<MOUL::NetMsgGameMessageDirected>();
                if (directedMsg->m_message->makeSafeForNet())
                    dm_propagate_to(host, netmsg, directedMsg->m_receivers);
            }
            break;
        case MOUL::ID_NetMsgTestAndSet:
            dm_test_and_set(host, msg->m_client, netmsg->Cast<MOUL::NetMsgTestAndSet>());
            break;
        case MOUL::ID_NetMsgMembersListReq:
            dm_send_members(host, msg->m_client);
            break;
        case MOUL::ID_NetMsgSDLState:
            dm_read_sdl(host, msg->m_client, netmsg->Cast<MOUL::NetMsgSDLState>(), false);
            break;
        case MOUL::ID_NetMsgSDLStateBCast:
            dm_read_sdl(host, msg->m_client, netmsg->Cast<MOUL::NetMsgSDLState>(), true);
            break;
        case MOUL::ID_NetMsgRelevanceRegions:
            //TODO: Filter messages by client's requested relevance regions
            break;
        case MOUL::ID_NetMsgLoadClone:
            pthread_mutex_lock(&host->m_clientMutex);
            msg->m_client->m_clientKey = netmsg->Cast<MOUL::NetMsgLoadClone>()->m_object;
            pthread_mutex_unlock(&host->m_clientMutex);
            dm_propagate(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
            break;
        case MOUL::ID_NetMsgPlayerPage:
            //TODO: Whatever the client is expecting us to do
            break;
        default:
            fprintf(stderr, "[Game] Warning: Unhandled message: %04X\n",
                    msg->m_messageType);
            break;
        }
    } catch (DS::SockHup) {
        // Client wasn't paying attention
    }
    netmsg->unref();
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void* dm_gameHost(void* hostp)
{
    GameHost_Private* host = reinterpret_cast<GameHost_Private*>(hostp);

    for ( ;; ) {
        DS::FifoMessage msg = host->m_channel.getMessage();
        try {
            switch (msg.m_messageType) {
            case e_GameShutdown:
                dm_game_shutdown(host);
                return 0;
            case e_GameCleanup:
                dm_game_cleanup(host);
                break;
            case e_GameJoinAge:
                dm_game_join(host, reinterpret_cast<Game_ClientMessage*>(msg.m_payload));
                break;
            case e_GamePropagate:
                dm_game_message(host, reinterpret_cast<Game_PropagateMessage*>(msg.m_payload));
                break;
            default:
                /* Invalid message...  This shouldn't happen */
                DS_DASSERT(0);
                break;
            }
        } catch (DS::AssertException ex) {
            fprintf(stderr, "[Game] Assertion failed at %s:%ld:  %s\n",
                    ex.m_file, ex.m_line, ex.m_cond);
            if (msg.m_payload) {
                // Keep clients from blocking on a reply
                SEND_REPLY(reinterpret_cast<Game_ClientMessage*>(msg.m_payload),
                           DS::e_NetInternalError);
            }
        }
    }

    dm_game_shutdown(host);
    return 0;
}

GameHost_Private* start_game_host(uint32_t ageMcpId)
{
    PGconn* postgres = PQconnectdb(DS::String::Format(
                    "host='%s' port='%s' user='%s' password='%s' dbname='%s'",
                    DS::Settings::DbHostname(), DS::Settings::DbPort(),
                    DS::Settings::DbUsername(), DS::Settings::DbPassword(),
                    DS::Settings::DbDbaseName()).c_str());
    if (PQstatus(postgres) != CONNECTION_OK) {
        fprintf(stderr, "Error connecting to postgres: %s", PQerrorMessage(postgres));
        PQfinish(postgres);
        return 0;
    }

    PostgresStrings<1> parms;
    parms.set(0, ageMcpId);
    PGresult* result = PQexecParams(postgres,
            "SELECT \"AgeUuid\", \"AgeFilename\", \"AgeIdx\", \"SdlIdx\""
            "    FROM game.\"Servers\" WHERE idx=$1",
            1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(postgres));
        PQclear(result);
        PQfinish(postgres);
        return 0;
    }
    if (PQntuples(result) == 0) {
        fprintf(stderr, "[Game] Age MCP %u not found\n", ageMcpId);
        PQclear(result);
        PQfinish(postgres);
        return 0;
    } else {
        DS_DASSERT(PQntuples(result) == 1);

        GameHost_Private* host = new GameHost_Private();
        pthread_mutex_init(&host->m_clientMutex, 0);
        host->m_instanceId = PQgetvalue(result, 0, 0);
        host->m_ageFilename = PQgetvalue(result, 0, 1);
        host->m_ageIdx = strtoul(PQgetvalue(result, 0, 2), 0, 10);
        host->m_serverIdx = ageMcpId;
        host->m_postgres = postgres;

        // Fetch the vault's SDL blob
        Auth_NodeInfo sdlFind;
        AuthClient_Private fakeClient;
        sdlFind.m_client = &fakeClient;
        sdlFind.m_node.set_NodeIdx(strtoul(PQgetvalue(result, 0, 3), 0, 10));
        s_authChannel.putMessage(e_VaultFetchNode, reinterpret_cast<void*>(&sdlFind));
        DS::FifoMessage reply = fakeClient.m_channel.getMessage();
        if (reply.m_messageType != DS::e_NetSuccess) {
            fprintf(stderr, "[Game] Error fetching Age SDL\n");
            PQclear(result);
            PQfinish(postgres);
            delete host;
            return 0;
        }
        host->m_sdlIdx = sdlFind.m_node.m_NodeIdx;
        host->m_ageSdl = sdlFind.m_node.m_Blob_1;

        pthread_mutex_lock(&s_gameHostMutex);
        s_gameHosts[ageMcpId] = host;
        pthread_mutex_unlock(&s_gameHostMutex);
        PQclear(result);

        pthread_t threadh;
        pthread_create(&threadh, 0, &dm_gameHost, reinterpret_cast<void*>(host));
        pthread_detach(threadh);
        return host;
    }
}