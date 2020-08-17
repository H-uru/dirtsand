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

#include "GameServer_Private.h"
#include "settings.h"
#include "errors.h"
#include <string_theory/format>
#include <dirent.h>
#include <sys/stat.h>
#include <poll.h>
#include <chrono>
#include <functional>

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
    if (size != 36)
        throw DS::InvalidConnectionHeader();
    DS::Uuid clientUuid, connUuid;
    DS::RecvBuffer(client.m_sock, clientUuid.m_bytes, sizeof(clientUuid.m_bytes));
    DS::RecvBuffer(client.m_sock, connUuid.m_bytes, sizeof(connUuid.m_bytes));

    /* Reply header */
    client.m_buffer.truncate();
    client.m_buffer.write<uint8_t>(DS::e_ServToCliEncrypt);

    /* Establish encryption, and write reply body */
    uint8_t msgId = DS::RecvValue<uint8_t>(client.m_sock);
    if (msgId != DS::e_CliToServConnect)
        throw DS::InvalidConnectionHeader();
    uint8_t msgSize = DS::RecvValue<uint8_t>(client.m_sock);
    if (msgSize == 2) {
        // no seed... client wishes unencrypted connection (that's okay, nobody
        // else can "fake" us as nobody has the private key, so if the client
        // actually wants encryption it will only work with the correct peer)
        client.m_buffer.write<uint8_t>(2); // reply with an empty seed as well
    } else {
        uint8_t Y[64];
        memset(Y, 0, sizeof(Y));
        if (msgSize > 66)
            throw DS::InvalidConnectionHeader();
        DS::RecvBuffer(client.m_sock, Y, 64 - (66 - msgSize));
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
    {
        std::lock_guard<std::mutex> gameHostGuard(s_gameHostMutex);
        hostmap_t::iterator host_iter = s_gameHosts.find(ageMcpId);
        if (host_iter != s_gameHosts.end())
            return host_iter->second;
    }
    try {
        return start_game_host(ageMcpId);
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Game] ERROR: {}\n", ex.what());
    }
    return nullptr;
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

    // Look up the server after we finish receiving the message, so we can
    // correctly send a reply if the server isn't found.
    uint32_t mcpId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);

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

    client.m_host = find_game_host(mcpId);
    if (!client.m_host) {
        ST::printf(stderr, "Could not find a game host for {}\n", mcpId);
        client.m_buffer.write<uint32_t>(DS::e_NetInternalError);
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

    client.m_host->m_clientMutex.lock();
    client.m_host->m_clients[client.m_clientInfo.m_PlayerId] = &client;
    client.m_host->m_clientMutex.unlock();
}

void cb_netmsg(GameClient_Private& client)
{
    Game_PropagateMessage msg;
    msg.m_client = &client;
    msg.m_messageType = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);

    uint32_t size = DS::CryptRecvSize(client.m_sock, client.m_crypt);
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, buffer.get(), size);
    msg.m_message = DS::Blob::Steal(buffer.release(), size);
    if (client.m_host) {
        client.m_host->m_channel.putMessage(e_GamePropagate, reinterpret_cast<void*>(&msg));
        client.m_channel.getMessage();
    } else {
        ST::printf(stderr, "Client {} sent a game message with no game host connection\n",
                   DS::SockIpAddress(client.m_sock));
        throw DS::SockHup();
    }
}

void cb_gameMgrMsg(GameClient_Private& client)
{
    uint32_t size = DS::CryptRecvSize(client.m_sock, client.m_crypt);
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, buffer.get(), size);

#ifdef DEBUG
    fputs("GAME MGR MSG", stdout);
    for (size_t i=0; i<size; ++i) {
        if ((i % 16) == 0)
            fputs("\n    ", stdout);
        else if ((i % 16) == 8)
            fputs("   ", stdout);
        ST::printf("{02X} ", buffer[i]);
    }
    fputc('\n', stdout);
#endif
}

void cb_sockRead(GameClient_Private& client)
{
    uint16_t msgId = DS::CryptRecvValue<uint16_t>(client.m_sock, client.m_crypt);
    switch (msgId) {
    case e_CliToGame_PingRequest:
        cb_ping(client);
        break;
    case e_CliToGame_JoinAgeRequest:
        cb_join(client);
        break;
    case e_CliToGame_PropagateBuffer:
        cb_netmsg(client);
        break;
    case e_CliToGame_GameMgrMsg:
        cb_gameMgrMsg(client);
        break;
    default:
        /* Invalid message */
        ST::printf(stderr, "[Game] Got invalid message ID {} from {}\n",
                   msgId, DS::SockIpAddress(client.m_sock));
        throw DS::SockHup();
    }
}

void cb_broadcast(GameClient_Private& client)
{
    DS::FifoMessage bcast = client.m_broadcast.getMessage();
    DS::BufferStream* msg = reinterpret_cast<DS::BufferStream*>(bcast.m_payload);
    START_REPLY(bcast.m_messageType);
    client.m_buffer.writeBytes(msg->buffer(), msg->size());
    msg->unref();
    SEND_REPLY();
}


void wk_gameWorker(DS::SocketHandle sockp)
{
    GameClient_Private client;
    client.m_sock = sockp;
    client.m_host = nullptr;
    client.m_crypt = nullptr;
    client.m_isLoaded = false;

    try {
        game_client_init(client);
    } catch (const DS::InvalidConnectionHeader& ex) {
        ST::printf(stderr, "[Game] Invalid connection header from {}\n",
                   DS::SockIpAddress(sockp));
        return;
    } catch (const DS::SockHup&) {
        // Socket closed...
        return;
    }

    try {
        // Poll the client socket and the host broadcast channel for messages
        pollfd fds[2];
        fds[0].fd = DS::SockFd(sockp);
        fds[0].events = POLLIN;
        fds[1].fd = client.m_broadcast.fd();
        fds[1].events = POLLIN;

        for ( ;; ) {
            int result = poll(fds, 2, NET_TIMEOUT * 1000);
            if (result < 0) {
                ST::printf(stderr, "[Game] Failed to poll for events: {}\n",
                           strerror(errno));
                throw DS::SockHup();
            }
            if (result == 0 || fds[0].revents & POLLHUP)
                throw DS::SockHup();

            if (fds[0].revents & POLLIN)
                cb_sockRead(client);
            if (fds[1].revents & POLLIN)
                cb_broadcast(client);
        }
    } catch (const DS::SockHup&) {
        // Socket closed...
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Game] Error processing client message from {}: {}\n",
                   DS::SockIpAddress(sockp), ex.what());
    }

    if (client.m_host) {
        client.m_host->m_clientMutex.lock();
        client.m_host->m_clients.erase(client.m_clientInfo.m_PlayerId);
        client.m_host->m_clientMutex.unlock();
        Game_ClientMessage msg;
        msg.m_client = &client;
        try {
            client.m_host->m_channel.putMessage(e_GameDisconnect, reinterpret_cast<void*>(&msg));
            client.m_channel.getMessage();
        } catch (const std::exception& ex) {
            ST::printf(stderr, "[Game] WARNING: {}\n", ex.what());
        }
    }

    // Drain the broadcast channel
    try {
        while (client.m_broadcast.hasMessage()) {
            DS::FifoMessage msg = client.m_broadcast.getMessage();
            reinterpret_cast<DS::BufferStream*>(msg.m_payload)->unref();
        }
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Game] WARNING: {}\n", ex.what());
    }

    DS::CryptStateFree(client.m_crypt);
    DS::FreeSock(client.m_sock);
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
        ST::string trimmed = ST::string(lnbuffer).before_first('#').trim();
        if (trimmed.empty())
            continue;
        std::vector<ST::string> line = trimmed.split('=');
        if (line.size() != 2) {
            ST::printf(stderr, "[Game] Invalid AGE line: \"{}\"", lnbuffer);
            continue;
        }
        if (line[0] == "StartDateTime") {
            age.m_startTime = line[1].to_uint(10);
        } else if (line[0] == "DayLength") {
            age.m_dayLength = line[1].to_double();
        } else if (line[0] == "MaxCapacity") {
            age.m_maxCapacity = line[1].to_uint(10);
        } else if (line[0] == "LingerTime") {
            age.m_lingerTime = line[1].to_uint(10);
        } else if (line[0] == "SequencePrefix") {
            age.m_seqPrefix = line[1].to_int(10);
        } else if (line[0] == "ReleaseVersion" || line[0] == "Page") {
            // Ignored
        } else {
            ST::printf(stderr, "[Game] Invalid AGE line: \"{}\"", lnbuffer);
        }
    }
    return age;
}

void DS::GameServer_Init()
{
    dirent** dirls;
    int count = scandir(DS::Settings::AgePath(), &dirls, &sel_age, &alphasort);
    if (count < 0) {
        ST::printf(stderr, "[Game] Error reading age descriptors: {}\n", strerror(errno));
    } else if (count == 0) {
        fputs("[Game] Warning: No age descriptors found!\n", stderr);
        free(dirls);
    } else {
        for (int i=0; i<count; ++i) {
            ST::string filename = ST::format("{}/{}", DS::Settings::AgePath(), dirls[i]->d_name);
            std::unique_ptr<FILE, std::function<int (FILE*)>> ageFile(fopen(filename.c_str(), "r"), &fclose);
            if (ageFile) {
                char magic[12];
                if (fread(magic, 1, 12, ageFile.get()) != 12) {
                    ST::printf(stderr, "[Game] Error: File {} is empty\n", filename);
                    break;
                }
                if (memcmp(magic, "whatdoyousee", 12) == 0 || memcmp(magic, "notthedroids", 12) == 0
                    || memcmp(magic, "BriceIsSmart", 12) == 0) {
                    fputs("[Game] Error: Please decrypt your .age files before using!\n", stderr);
                    break;
                }
                fseek(ageFile.get(), 0, SEEK_SET);

                ST::string ageName = dirls[i]->d_name;
                ageName = ageName.before_first(".age");
                Game_AgeInfo age = age_parse(ageFile.get());
                if (age.m_seqPrefix >= 0)
                    s_ages[ageName] = age;
            }
            free(dirls[i]);
        }
        free(dirls);
    }
}

void DS::GameServer_Add(DS::SocketHandle client)
{
#ifdef DEBUG
    if (s_commdebug)
        ST::printf("Connecting GAME on {}\n", DS::SockIpAddress(client));
#endif

    std::thread threadh(&wk_gameWorker, client);
    threadh.detach();
}

void DS::GameServer_Shutdown()
{
    {
        std::lock_guard<std::mutex> gameHostGuard(s_gameHostMutex);
        hostmap_t::iterator host_iter;
        for (host_iter = s_gameHosts.begin(); host_iter != s_gameHosts.end(); ++host_iter) {
            try {
                host_iter->second->m_channel.putMessage(e_GameShutdown);
            } catch (const std::exception& ex) {
                ST::printf(stderr, "[Game] WARNING: {}\n", ex.what());
            }
        }
    }

    bool complete = false;
    for (int i=0; i<50 && !complete; ++i) {
        s_gameHostMutex.lock();
        size_t alive = s_gameHosts.size();
        s_gameHostMutex.unlock();
        if (alive == 0)
            complete = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!complete)
        fputs("[Game] Servers didn't die after 5 seconds!\n", stderr);
}

void DS::GameServer_UpdateGlobalSDL(const ST::string& age)
{
    std::lock_guard<std::mutex> lock(s_gameHostMutex);
    for (auto it = s_gameHosts.begin(); it != s_gameHosts.end(); ++it) {
        if (!it->second || it->second->m_ageFilename != age)
            continue;
        try {
            it->second->m_channel.putMessage(e_GameGlobalSdlUpdate, nullptr);
        } catch (const std::exception& ex) {
            ST::printf(stderr, "[Game] WARNING: {}\n", ex.what());
        }
    }
}


bool DS::GameServer_UpdateVaultSDL(const DS::Vault::Node& node, uint32_t ageMcpId)
{
    s_gameHostMutex.lock();
    hostmap_t::iterator host_iter = s_gameHosts.find(ageMcpId);
    GameHost_Private* host = nullptr;
    if (host_iter != s_gameHosts.end())
        host = host_iter->second;
    s_gameHostMutex.unlock();

    if (host) {
        Game_SdlMessage* msg = new Game_SdlMessage;
        msg->m_node = node.copy();
        try {
            host->m_channel.putMessage(e_GameLocalSdlUpdate, msg);
            return true;
        } catch (const std::exception& ex) {
            ST::printf(stderr, "[Game] WARNING: {}\n", ex.what());
        }
    }
    return false;
}

void DS::GameServer_DisplayClients()
{
    std::lock_guard<std::mutex> gameHostGuard(s_gameHostMutex);
    if (s_gameHosts.size())
        fputs("Game Servers:\n", stdout);
    for (hostmap_t::iterator host_iter = s_gameHosts.begin(); host_iter != s_gameHosts.end(); ++host_iter) {
        ST::printf("    {} {}\n", host_iter->second->m_ageFilename,
                   host_iter->second->m_instanceId.toString(true));
        std::lock_guard<std::mutex> clientGuard(host_iter->second->m_clientMutex);
        for (auto client_iter = host_iter->second->m_clients.begin();
             client_iter != host_iter->second->m_clients.end(); ++ client_iter)
            ST::printf("      * {} - {} ({})\n", DS::SockIpAddress(client_iter->second->m_sock),
                       client_iter->second->m_clientInfo.m_PlayerName,
                       client_iter->second->m_clientInfo.m_PlayerId);
    }
}

uint32_t DS::GameServer_GetNumClients(Uuid instance)
{
    std::lock_guard<std::mutex> gameHostGuard(s_gameHostMutex);
    for (auto it = s_gameHosts.begin(); it != s_gameHosts.end(); ++it) {
        if (it->second->m_instanceId == instance)
            return it->second->m_clients.size();
    }
    return 0;
}
