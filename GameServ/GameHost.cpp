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
#include "PlasMOUL/NetMessages/NetMessage.h"
#include "errors.h"

hostmap_t s_gameHosts;
pthread_mutex_t s_gameHostMutex;

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

void dm_propagate(GameHost_Private* host, MOUL::Creatable* msg)
{
    host->m_buffer.truncate();
    host->m_buffer.write<uint16_t>(e_GameToCli_PropagateBuffer);
    host->m_buffer.write<uint32_t>(msg->type());
    host->m_buffer.write<uint32_t>(0);
    MOUL::Factory::WriteCreatable(&host->m_buffer, msg);
    host->m_buffer.seek(6, SEEK_SET);
    host->m_buffer.write<uint32_t>(host->m_buffer.size() - 10);

    pthread_mutex_lock(&host->m_clientMutex);
    std::list<GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        DS::CryptSendBuffer((*client_iter)->m_sock, (*client_iter)->m_crypt,
                            host->m_buffer.buffer(), host->m_buffer.size());
    }
    pthread_mutex_unlock(&host->m_clientMutex);
}

void dm_propagate(GameHost_Private* host, DS::Blob cooked, uint32_t msgType)
{
    host->m_buffer.truncate();
    host->m_buffer.write<uint16_t>(e_GameToCli_PropagateBuffer);
    host->m_buffer.write<uint32_t>(msgType);
    host->m_buffer.write<uint32_t>(cooked.size());
    host->m_buffer.writeBytes(cooked.buffer(), cooked.size());

    pthread_mutex_lock(&host->m_clientMutex);
    std::list<GameClient_Private*>::iterator client_iter;
    for (client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        DS::CryptSendBuffer((*client_iter)->m_sock, (*client_iter)->m_crypt,
                            host->m_buffer.buffer(), host->m_buffer.size());
    }
    pthread_mutex_unlock(&host->m_clientMutex);
}

void dm_game_message(GameHost_Private* host, Game_PropagateMessage* msg)
{
    DS::BlobStream stream(msg->m_message);
    MOUL::NetMessage* netmsg;
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

    switch (msg->m_messageType) {
    case MOUL::ID_NetMsgLoadClone:
        dm_propagate(host, msg->m_message, msg->m_messageType);
        break;
    case MOUL::ID_NetMsgPlayerPage:
        //dm_propagate(host, msg->m_message, msg->m_messageType);
        break;
    default:
        fprintf(stderr, "[Game] Warning: Unhandled message: %04X\n",
                msg->m_messageType);
#ifdef DEBUG
        // In debug builds, we'll just go ahead and blindly propagate
        // any unknown message type
        dm_propagate(host, msg->m_message, msg->m_messageType);
#endif
        break;
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

GameHost_Private* start_game_host(const DS::Uuid& ageId)
{
    GameHost_Private* host = new GameHost_Private();
    pthread_mutex_init(&host->m_clientMutex, 0);
    host->m_instanceId = ageId;

    pthread_mutex_lock(&s_gameHostMutex);
    s_gameHosts[ageId] = host;
    pthread_mutex_unlock(&s_gameHostMutex);

    pthread_t threadh;
    pthread_create(&threadh, 0, &dm_gameHost, reinterpret_cast<void*>(host));
    pthread_detach(threadh);
    return host;
}
