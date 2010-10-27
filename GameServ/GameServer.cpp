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
#include "settings.h"
#include "errors.h"

extern bool s_commdebug;

enum GameServer_MsgIds
{
    e_CliToGame_PingRequest = 0, e_CliToGame_JoinAgeRequest,
    e_CliToGame_Propagatebuffer, e_CliToGame_GameMgrMsg,

    e_GameToCli_PingReply = 0, e_GameToCli_JoinAgeReply,
    e_GameToCli_PropagateBuffer, e_GameToCli_GameMgrMsg,
};

#define START_REPLY(msgId) \
    client.m_buffer.truncate(); \
    client.m_buffer.write<uint16_t>(msgId)

#define SEND_REPLY() \
    DS::CryptSendBuffer(client.m_sock, client.m_crypt, \
                        client.m_buffer.buffer(), client.m_buffer.size())

DS::Uuid game_client_init(GameClient_Private& client)
{
    /* Game client header:  size, account uuid, age instance uuid */
    uint32_t size = DS::RecvValue<uint32_t>(client.m_sock);
    DS_PASSERT(size == 36);
    DS::Uuid clientUuid, ageUuid;
    DS::RecvBuffer(client.m_sock, clientUuid.m_bytes, sizeof(clientUuid.m_bytes));
    DS::RecvBuffer(client.m_sock, ageUuid.m_bytes, sizeof(ageUuid.m_bytes));

    /* Establish encryption */
    uint8_t msgId = DS::RecvValue<uint8_t>(client.m_sock);
    DS_PASSERT(msgId == DS::e_CliToServConnect);
    uint8_t msgSize = DS::RecvValue<uint8_t>(client.m_sock);
    DS_PASSERT(msgSize == 66);

    uint8_t Y[64];
    DS::RecvBuffer(client.m_sock, Y, 64);
    BYTE_SWAP_BUFFER(Y, 64);

    uint8_t serverSeed[7];
    uint8_t sharedKey[7];
    DS::CryptEstablish(serverSeed, sharedKey, DS::Settings::CryptKey(DS::e_KeyGame_N),
                       DS::Settings::CryptKey(DS::e_KeyGame_K), Y);

    client.m_buffer.truncate();
    client.m_buffer.write<uint8_t>(DS::e_ServToCliEncrypt);
    client.m_buffer.write<uint8_t>(9);
    client.m_buffer.writeBytes(serverSeed, 7);
    DS::SendBuffer(client.m_sock, client.m_buffer.buffer(), client.m_buffer.size());

    client.m_crypt = DS::CryptStateInit(sharedKey, 7);
    return ageUuid;
}

GameHost_Private* find_game_host(const DS::Uuid& ageUuid)
{
    pthread_mutex_lock(&s_gameHostMutex);
    hostmap_t::iterator host_iter = s_gameHosts.find(ageUuid);
    GameHost_Private* host = 0;
    if (host_iter != s_gameHosts.end())
        host = host_iter->second;
    pthread_mutex_unlock(&s_gameHostMutex);
    return host ? host : start_game_host(ageUuid);
}

void cb_ping(GameClient_Private& client)
{
    START_REPLY(e_GameToCli_PingReply);

    // Ping time
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    SEND_REPLY();
}

void* wk_gameWorker(void* sockp)
{
    GameClient_Private client;
    client.m_sock = reinterpret_cast<DS::SocketHandle>(sockp);

    DS::Uuid ageUuid;
    try {
        ageUuid = game_client_init(client);
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[Game] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
        return 0;
    } catch (DS::SockHup) {
        // Socket closed...
        return 0;
    }

    GameHost_Private* host = find_game_host(ageUuid);
    pthread_mutex_lock(&host->m_clientMutex);
    host->m_clients.push_back(&client);
    pthread_mutex_unlock(&host->m_clientMutex);

    try {
        for ( ;; ) {
            uint16_t msgId = DS::CryptRecvValue<uint16_t>(client.m_sock, client.m_crypt);
            switch (msgId) {
            case e_CliToGame_PingRequest:
                cb_ping(client);
                break;
            default:
                /* Invalid message */
                fprintf(stderr, "[Game] Got invalid message ID %d from %s\n",
                        msgId, DS::SockIpAddress(client.m_sock).c_str());
                DS::CloseSock(client.m_sock);
                throw DS::SockHup();
            }
        }
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[Game] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
    } catch (DS::SockHup) {
        // Socket closed...
    }

    pthread_mutex_lock(&host->m_clientMutex);
    std::list<GameClient_Private*>::iterator client_iter = host->m_clients.begin();
    while (client_iter != host->m_clients.end()) {
        if (*client_iter == &client)
            client_iter = host->m_clients.erase(client_iter);
        else
            ++client_iter;
    }
    pthread_mutex_unlock(&host->m_clientMutex);
    host->m_channel.putMessage(e_GameCleanup);

    DS::CryptStateFree(client.m_crypt);
    DS::FreeSock(client.m_sock);
    return 0;
}

void DS::GameServer_Init()
{
    pthread_mutex_init(&s_gameHostMutex, 0);
}

void DS::GameServer_Add(DS::SocketHandle client)
{
#ifdef DEBUG
    if (s_commdebug)
        printf("Connecting GAME on %s\n", DS::SockIpAddress(client).c_str());
#endif

    pthread_t threadh;
    pthread_create(&threadh, 0, &wk_gameWorker, reinterpret_cast<void*>(client));
    pthread_detach(threadh);
}

void DS::GameServer_Shutdown()
{
    pthread_mutex_lock(&s_gameHostMutex);
    hostmap_t::iterator host_iter;
    for (host_iter = s_gameHosts.begin(); host_iter != s_gameHosts.end(); ++host_iter)
        host_iter->second->m_channel.putMessage(e_GameShutdown);
    pthread_mutex_unlock(&s_gameHostMutex);

    bool complete = false;
    for (int i=0; i<50 && !complete; ++i) {
        pthread_mutex_lock(&s_gameHostMutex);
        size_t alive = s_gameHosts.size();
        pthread_mutex_unlock(&s_gameHostMutex);
        if (alive == 0)
            complete = true;
        usleep(100000);
    }
    if (!complete)
        fprintf(stderr, "[Game] Servers didn't die after 5 seconds!\n");
    pthread_mutex_destroy(&s_gameHostMutex);
}

void DS::GameServer_DisplayClients()
{
    pthread_mutex_lock(&s_gameHostMutex);
    if (s_gameHosts.size())
        printf("Game Servers:\n");
    for (hostmap_t::iterator host_iter = s_gameHosts.begin();
         host_iter != s_gameHosts.end(); ++host_iter) {
        printf("    {%s}\n", host_iter->second->m_instanceId.toString().c_str());
    }
    pthread_mutex_unlock(&s_gameHostMutex);
}
