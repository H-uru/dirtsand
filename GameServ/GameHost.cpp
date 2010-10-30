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
#include "PlasMOUL/Messages/ServerReplyMsg.h"
#include "errors.h"

hostmap_t s_gameHosts;
pthread_mutex_t s_gameHostMutex;
agemap_t s_ages;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

void dm_game_shutdown(GameHost_Private* host)
{
    pthread_mutex_lock(&host->m_clientMutex);
    std::list<GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter)
        DS::CloseSock((*client_iter)->m_sock);
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
    delete host;
}

void dm_game_cleanup(GameHost_Private* host)
{
    //TODO: shutdown this host if no clients connect within a time limit
}

void dm_game_join(GameHost_Private* host, Game_ClientMessage* msg)
{
    //TODO: announce the new player
    SEND_REPLY(msg, DS::e_NetSuccess);
}

#define DM_WRITEMSG(host, msg) \
    host->m_buffer.truncate(); \
    host->m_buffer.write<uint16_t>(e_GameToCli_PropagateBuffer); \
    host->m_buffer.write<uint32_t>(msg->type()); \
    host->m_buffer.write<uint32_t>(0); \
    MOUL::Factory::WriteCreatable(&host->m_buffer, msg); \
    host->m_buffer.seek(6, SEEK_SET); \
    host->m_buffer.write<uint32_t>(host->m_buffer.size() - 10)

void dm_propagate(GameHost_Private* host, MOUL::Creatable* msg, uint32_t sender)
{
    DM_WRITEMSG(host, msg);

    pthread_mutex_lock(&host->m_clientMutex);
    std::list<GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if ((*client_iter)->m_clientInfo.m_PlayerId == sender)
            continue;
        try {
            DS::CryptSendBuffer((*client_iter)->m_sock, (*client_iter)->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
        } catch (DS::SockHup) {
            // This is handled below too, but we don't want to skip the rest
            // of the client list if one hung up
        }
    }
    pthread_mutex_unlock(&host->m_clientMutex);
}

void dm_propagate(GameHost_Private* host, DS::Blob cooked, uint32_t msgType, uint32_t sender)
{
    host->m_buffer.truncate();
    host->m_buffer.write<uint16_t>(e_GameToCli_PropagateBuffer);
    host->m_buffer.write<uint32_t>(msgType);
    host->m_buffer.write<uint32_t>(cooked.size());
    host->m_buffer.writeBytes(cooked.buffer(), cooked.size());

    pthread_mutex_lock(&host->m_clientMutex);
    std::list<GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if ((*client_iter)->m_clientInfo.m_PlayerId == sender)
            continue;
        try {
            DS::CryptSendBuffer((*client_iter)->m_sock, (*client_iter)->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
        } catch (DS::SockHup) {
            // This is handled below too, but we don't want to skip the rest
            // of the client list if one hung up
        }
    }
    pthread_mutex_unlock(&host->m_clientMutex);
}

void dm_send_state(GameHost_Private* host, GameClient_Private* client)
{
    //TODO: Send saved SDL states

    MOUL::NetMsgInitialAgeStateSent* reply = MOUL::NetMsgInitialAgeStateSent::Create();
    reply->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                          | MOUL::NetMessage::e_IsSystemMessage
                          | MOUL::NetMessage::e_NeedsReliableSend;
    reply->m_timestamp.setNow();
    reply->m_numStates = 0;

    DM_WRITEMSG(host, reply);
    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
    reply->unref();
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
    std::list<GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if ((*client_iter)->m_clientInfo.m_PlayerId != client->m_clientInfo.m_PlayerId
            && !client->m_clientKey.isNull()) {
            MOUL::NetMsgMemberInfo info;
            info.m_client = (*client_iter)->m_clientInfo;
            info.m_avatarKey = (*client_iter)->m_clientKey;
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
        case MOUL::ID_NetMsgGameStateRequest:
            dm_send_state(host, msg->m_client);
            break;
        case MOUL::ID_NetMsgGameMessage:
            {
                MOUL::NetMsgGameMessage* gameMsg = netmsg->Cast<MOUL::NetMsgGameMessage>();
                switch (gameMsg->m_message->type())
                {
                case MOUL::ID_NotifyMsg:
                case MOUL::ID_AvatarInputStateMsg:
                case MOUL::ID_InputIfaceMgrMsg:
                    dm_propagate(host, msg->m_message, msg->m_messageType,
                                 msg->m_client->m_clientInfo.m_PlayerId);
                    break;
                default:
                    fprintf(stderr, "[Game] Warning: Unhandled game message: %04X\n",
                            gameMsg->m_message->type());
                }
            }
            break;
        case MOUL::ID_NetMsgTestAndSet:
            dm_test_and_set(host, msg->m_client, netmsg->Cast<MOUL::NetMsgTestAndSet>());
            break;
        case MOUL::ID_NetMsgMembersListReq:
            dm_send_members(host, msg->m_client);
            break;
        case MOUL::ID_NetMsgLoadClone:
            pthread_mutex_lock(&host->m_clientMutex);
            msg->m_client->m_clientKey = netmsg->Cast<MOUL::NetMsgLoadClone>()->m_object;
            pthread_mutex_unlock(&host->m_clientMutex);
            dm_propagate(host, msg->m_message, msg->m_messageType,
                         msg->m_client->m_clientInfo.m_PlayerId);
            break;
        case MOUL::ID_NetMsgPlayerPage:
            // Do we care?
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
    AuthClient_Private tempClient;
    Auth_GameAge ageMsg;
    ageMsg.m_client = &tempClient;
    ageMsg.m_mcpId = ageMcpId;
    s_authChannel.putMessage(e_AuthFetchAgeServer, reinterpret_cast<void*>(&ageMsg));

    DS::FifoMessage ageReply = tempClient.m_channel.getMessage();
    if (ageReply.m_messageType == DS::e_NetSuccess) {
        GameHost_Private* host = new GameHost_Private();
        pthread_mutex_init(&host->m_clientMutex, 0);
        host->m_instanceId = ageMsg.m_instanceId;
        host->m_ageFilename = ageMsg.m_name;
        host->m_ageIdx = ageMsg.m_ageNodeIdx;

        pthread_mutex_lock(&s_gameHostMutex);
        s_gameHosts[ageMcpId] = host;
        pthread_mutex_unlock(&s_gameHostMutex);

        pthread_t threadh;
        pthread_create(&threadh, 0, &dm_gameHost, reinterpret_cast<void*>(host));
        pthread_detach(threadh);
        return host;
    } else if (ageReply.m_messageType == DS::e_NetAgeNotFound) {
        fprintf(stderr, "[Game] Age MCP %u not found\n", ageMcpId);
    }
    return 0;
}
