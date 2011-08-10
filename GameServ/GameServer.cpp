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
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

extern bool s_commdebug;

#define START_REPLY(msgId) \
    client.m_buffer.truncate(); \
    client.m_buffer.write<uint16_t>(msgId)

#define SEND_REPLY() \
    DS::CryptSendBuffer(client.m_sock, client.m_crypt, \
                        client.m_buffer.buffer(), client.m_buffer.size())

void game_client_init(GameClient_Private& client)
{
    /* Game client header:  size, account uuid, age instance uuid */
    uint32_t size = DS::RecvValue<uint32_t>(client.m_sock);
    DS_PASSERT(size == 36);
    DS::Uuid clientUuid, connUuid;
    DS::RecvBuffer(client.m_sock, clientUuid.m_bytes, sizeof(clientUuid.m_bytes));
    DS::RecvBuffer(client.m_sock, connUuid.m_bytes, sizeof(connUuid.m_bytes));

    /* Reply header */
    client.m_buffer.truncate();
    client.m_buffer.write<uint8_t>(DS::e_ServToCliEncrypt);

    /* Establish encryption, and write reply body */
    uint8_t msgId = DS::RecvValue<uint8_t>(client.m_sock);
    DS_PASSERT(msgId == DS::e_CliToServConnect);
    uint8_t msgSize = DS::RecvValue<uint8_t>(client.m_sock);
    if (msgSize == 2) { // no seed... client wishes unencrypted connection (that's okay, nobody else can "fake" us as nobody has the private key, so if the client actually wants encryption it will only work with the correct peer)
        client.m_buffer.write<uint8_t>(2); // reply with an empty seed as well
    }
    else {
        uint8_t Y[64];
        DS_PASSERT(msgSize == 66);
        DS::RecvBuffer(client.m_sock, Y, 64);
        BYTE_SWAP_BUFFER(Y, 64);

        uint8_t serverSeed[7];
        uint8_t sharedKey[7];
        DS::CryptEstablish(serverSeed, sharedKey, DS::Settings::CryptKey(DS::e_KeyGame_N),
                        DS::Settings::CryptKey(DS::e_KeyGame_K), Y);
        client.m_crypt = DS::CryptStateInit(sharedKey, 7);

        client.m_buffer.write<uint8_t>(9);
        client.m_buffer.writeBytes(serverSeed, 7);
    }
    
    /* send reply */
    DS::SendBuffer(client.m_sock, client.m_buffer.buffer(), client.m_buffer.size());
}

GameHost_Private* find_game_host(uint32_t ageMcpId)
{
    pthread_mutex_lock(&s_gameHostMutex);
    hostmap_t::iterator host_iter = s_gameHosts.find(ageMcpId);
    GameHost_Private* host = 0;
    if (host_iter != s_gameHosts.end())
        host = host_iter->second;
    pthread_mutex_unlock(&s_gameHostMutex);
    return host ? host : start_game_host(ageMcpId);
}

void cb_ping(GameClient_Private& client)
{
    START_REPLY(e_GameToCli_PingReply);

    // Ping time
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    SEND_REPLY();
}

void cb_join(GameClient_Private& client)
{
    START_REPLY(e_GameToCli_JoinAgeReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    uint32_t mcpId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_host = find_game_host(mcpId);
    DS_PASSERT(client.m_host != 0);

    Game_ClientMessage msg;
    msg.m_client = &client;
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, client.m_clientId.m_bytes,
                        sizeof(client.m_clientId.m_bytes));
    client.m_clientInfo.set_PlayerId(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));
    if (client.m_clientInfo.m_PlayerId == 0) {
        client.m_buffer.write<uint32_t>(DS::e_NetInvalidParameter);
        SEND_REPLY();
        return;
    }

    // Get player info from the vault
    Auth_NodeInfo nodeInfo;
    nodeInfo.m_client = &client;
    nodeInfo.m_node.set_NodeIdx(client.m_clientInfo.m_PlayerId);
    s_authChannel.putMessage(e_VaultFetchNode, reinterpret_cast<void*>(&nodeInfo));

    DS::FifoMessage reply = client.m_channel.getMessage();
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(reply.m_messageType);
        SEND_REPLY();
        return;
    }
    msg.m_client->m_clientInfo.set_PlayerName(nodeInfo.m_node.m_IString64_1);
    msg.m_client->m_clientInfo.set_CCRLevel(0);
    client.m_host->m_channel.putMessage(e_GameJoinAge, reinterpret_cast<void*>(&msg));

    reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);

    SEND_REPLY();

    pthread_mutex_lock(&client.m_host->m_clientMutex);
    client.m_host->m_clients[client.m_clientInfo.m_PlayerId] = &client;
    pthread_mutex_unlock(&client.m_host->m_clientMutex);
}

void cb_netmsg(GameClient_Private& client)
{
    Game_PropagateMessage msg;
    msg.m_client = &client;
    msg.m_messageType = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);

    uint32_t size = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    uint8_t* buffer = new uint8_t[size];
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, buffer, size);
    msg.m_message = DS::Blob::Steal(buffer, size);
    client.m_host->m_channel.putMessage(e_GamePropagate, reinterpret_cast<void*>(&msg));
    client.m_channel.getMessage();
}

void cb_gameMgrMsg(GameClient_Private& client)
{
    uint32_t size = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    uint8_t* buffer = new uint8_t[size];
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, buffer, size);

    printf("GAME MGR MSG");
    for (size_t i=0; i<size; ++i) {
        if ((i % 16) == 0)
            printf("\n    ");
        else if ((i % 16) == 8)
            printf("   ");
        printf("%02X ", buffer[i]);
    }
    printf("\n");
    delete[] buffer;
}

void* wk_gameWorker(void* sockp)
{
    GameClient_Private client;
    client.m_sock = reinterpret_cast<DS::SocketHandle>(sockp);
    client.m_host = 0;
    client.m_crypt = 0;

    try {
        game_client_init(client);
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[Game] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
        return 0;
    } catch (DS::SockHup) {
        // Socket closed...
        return 0;
    }

    try {
        for ( ;; ) {
            uint16_t msgId = DS::CryptRecvValue<uint16_t>(client.m_sock, client.m_crypt);
            switch (msgId) {
            case e_CliToGame_PingRequest:
                cb_ping(client);
                break;
            case e_CliToGame_JoinAgeRequest:
                cb_join(client);
                break;
            case e_CliToGame_Propagatebuffer:
                DS_PASSERT(client.m_host != 0);
                cb_netmsg(client);
                break;
            case e_CliToGame_GameMgrMsg:
                DS_PASSERT(client.m_host != 0);
                cb_gameMgrMsg(client);
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

    if (client.m_host) {
        pthread_mutex_lock(&client.m_host->m_clientMutex);
        client.m_host->m_clients.erase(client.m_clientInfo.m_PlayerId);
        pthread_mutex_unlock(&client.m_host->m_clientMutex);
        Game_ClientMessage msg;
        msg.m_client = &client;
        client.m_host->m_channel.putMessage(e_GameDisconnect, reinterpret_cast<void*>(&msg));
        client.m_channel.getMessage();
    }

    DS::CryptStateFree(client.m_crypt);
    DS::FreeSock(client.m_sock);
    return 0;
}

static int sel_age(const dirent* de)
{
    return strcmp(strrchr(de->d_name, '.'), ".age") == 0;
}

Game_AgeInfo age_parse(FILE* stream)
{
    char lnbuffer[4096];

    Game_AgeInfo age;
    while (fgets(lnbuffer, 4096, stream)) {
        std::vector<DS::String> line = DS::String(lnbuffer).strip('#').split('=');
        if (line.size() == 0)
            continue;
        if (line.size() != 2) {
            fprintf(stderr, "[Game] Invalid AGE line: %s", lnbuffer);
            continue;
        }
        if (line[0] == "StartDateTime") {
            age.m_startTime = line[1].toUint(10);
        } else if (line[0] == "DayLength") {
            age.m_dayLength = line[1].toDouble();
        } else if (line[0] == "MaxCapacity") {
            age.m_maxCapacity = line[1].toUint(10);
        } else if (line[0] == "LingerTime") {
            age.m_lingerTime = line[1].toUint(10);
        } else if (line[0] == "SequencePrefix") {
            age.m_seqPrefix = line[1].toInt(10);
        } else if (line[0] == "ReleaseVersion" || line[0] == "Page") {
            // Ignored
        } else {
            fprintf(stderr, "[Game] Invalid AGE line: %s", lnbuffer);
        }
    }
    return age;
}

void DS::GameServer_Init()
{
    dirent** dirls;
    int count = scandir(DS::Settings::AgePath(), &dirls, &sel_age, &alphasort);
    if (count < 0) {
        fprintf(stderr, "[Game] Error reading age descriptors: %s\n", strerror(errno));
    } else if (count == 0) {
        fprintf(stderr, "[Game] Warning: No age descriptors found!\n");
        free(dirls);
    } else {
        for (int i=0; i<count; ++i) {
            DS::String filename = DS::String::Format("%s/%s", DS::Settings::AgePath(), dirls[i]->d_name);
            FILE* ageFile = fopen(filename.c_str(), "r");
            if (ageFile) {
                char magic[12];
                fread(magic, 1, 12, ageFile);
                if (memcmp(magic, "whatdoyousee", 12) == 0 || memcmp(magic, "notthedroids", 12) == 0
                    || memcmp(magic, "BriceIsSmart", 12) == 0) {
                    fprintf(stderr, "[Game] Error: Please decrypt your .age files before using!\n");
                    break;
                }
                fseek(ageFile, 0, SEEK_SET);

                DS::String ageName = dirls[i]->d_name;
                ageName = ageName.left(ageName.find(".age"));
                Game_AgeInfo age = age_parse(ageFile);
                if (age.m_seqPrefix >= 0)
                    s_ages[ageName] = age;
                fclose(ageFile);
            }
            free(dirls[i]);
        }
        free(dirls);
    }

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
        for (auto client_iter = host_iter->second->m_clients.begin();
             client_iter != host_iter->second->m_clients.end(); ++ client_iter)
            printf("      * %s - %s (%u)\n", DS::SockIpAddress(client_iter->second->m_sock).c_str(),
                   client_iter->second->m_clientInfo.m_PlayerName.c_str(),
                   client_iter->second->m_clientInfo.m_PlayerId);
    }
    pthread_mutex_unlock(&s_gameHostMutex);
}
