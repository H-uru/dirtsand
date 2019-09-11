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

#include "GateServ.h"
#include "NetIO/CryptIO.h"
#include "Types/Uuid.h"
#include "settings.h"
#include "streams.h"
#include "errors.h"
#include <list>
#include <thread>
#include <mutex>
#include <chrono>

struct GateKeeper_Private
{
    DS::SocketHandle m_sock;
    DS::CryptState m_crypt;
    DS::BufferStream m_buffer;
};

static std::list<GateKeeper_Private*> s_clients;
static std::mutex s_clientMutex;

enum GateKeeper_MsgIds
{
    e_CliToGateKeeper_PingRequest = 0, e_CliToGateKeeper_FileServIpAddressRequest,
    e_CliToGateKeeper_AuthServIpAddressRequest,

    e_GateKeeperToCli_PingReply = 0, e_GateKeeperToCli_FileServIpAddressReply,
    e_GateKeeperToCli_AuthServIpAddressReply,
};

#define START_REPLY(msgId) \
    client.m_buffer.truncate(); \
    client.m_buffer.write<uint16_t>(msgId)

#define SEND_REPLY() \
    DS::CryptSendBuffer(client.m_sock, client.m_crypt, \
                        client.m_buffer.buffer(), client.m_buffer.size())

void gate_init(GateKeeper_Private& client)
{
    /* Gate Keeper header:  size, null uuid */
    uint32_t size = DS::RecvValue<uint32_t>(client.m_sock);
    if (size != 20)
        throw DS::InvalidConnectionHeader();
    DS::Uuid uuid;
    DS::RecvBuffer(client.m_sock, uuid.m_bytes, sizeof(uuid.m_bytes));

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
        DS::CryptEstablish(serverSeed, sharedKey, DS::Settings::CryptKey(DS::e_KeyGate_N),
                           DS::Settings::CryptKey(DS::e_KeyGate_K), Y);
        client.m_crypt = DS::CryptStateInit(sharedKey, 7);

        client.m_buffer.write<uint8_t>(9);
        client.m_buffer.writeBytes(serverSeed, 7);
    }

    /* send reply */
    DS::SendBuffer(client.m_sock, client.m_buffer.buffer(), client.m_buffer.size());
}

void cb_ping(GateKeeper_Private& client)
{
    START_REPLY(e_GateKeeperToCli_PingReply);

    // Ping time
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    // Payload
    uint32_t payloadSize = DS::CryptRecvSize(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(payloadSize);
    if (payloadSize) {
        std::unique_ptr<uint8_t[]> payload(new uint8_t[payloadSize]);
        DS::CryptRecvBuffer(client.m_sock, client.m_crypt, payload.get(), payloadSize);
        client.m_buffer.writeBytes(payload.get(), payloadSize);
    }

    SEND_REPLY();
}

void cb_fileServIpAddress(GateKeeper_Private& client)
{
    START_REPLY(e_GateKeeperToCli_FileServIpAddressReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    // From Patcher? (ignored)
    DS::CryptRecvValue<uint8_t>(client.m_sock, client.m_crypt);

    // Address
    ST::utf16_buffer address = DS::Settings::FileServerAddress();
    client.m_buffer.write<uint16_t>(address.size());
    client.m_buffer.writeBytes(address.data(), address.size() * sizeof(char16_t));

    SEND_REPLY();
}

void cb_authServIpAddress(GateKeeper_Private& client)
{
    START_REPLY(e_GateKeeperToCli_AuthServIpAddressReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    // Address
    ST::utf16_buffer address = DS::Settings::AuthServerAddress();
    client.m_buffer.write<uint16_t>(address.size());
    client.m_buffer.writeBytes(address.data(), address.size() * sizeof(char16_t));

    SEND_REPLY();
}

void wk_gateKeeper(DS::SocketHandle sockp)
{
    GateKeeper_Private client;
    client.m_crypt = nullptr;

    s_clientMutex.lock();
    client.m_sock = sockp;
    s_clients.push_back(&client);
    s_clientMutex.unlock();

    try {
        gate_init(client);

        for ( ;; ) {
            uint16_t msgId = DS::CryptRecvValue<uint16_t>(client.m_sock, client.m_crypt);
            switch (msgId) {
            case e_CliToGateKeeper_PingRequest:
                cb_ping(client);
                break;
            case e_CliToGateKeeper_FileServIpAddressRequest:
                cb_fileServIpAddress(client);
                break;
            case e_CliToGateKeeper_AuthServIpAddressRequest:
                cb_authServIpAddress(client);
                break;
            default:
                /* Invalid message */
                fprintf(stderr, "[GateKeeper] Got invalid message ID %d from %s\n",
                        msgId, DS::SockIpAddress(client.m_sock).c_str());
                DS::CloseSock(client.m_sock);
                throw DS::SockHup();
            }
        }
    } catch (const DS::SockHup&) {
        // Socket closed...
    } catch (const std::exception& ex) {
        fprintf(stderr, "[GateKeeper] Error processing client message from %s: %s\n",
                DS::SockIpAddress(sockp).c_str(), ex.what());
    }

    s_clientMutex.lock();
    auto client_iter = s_clients.begin();
    while (client_iter != s_clients.end()) {
        if (*client_iter == &client)
            client_iter = s_clients.erase(client_iter);
        else
            ++client_iter;
    }
    s_clientMutex.unlock();

    DS::CryptStateFree(client.m_crypt);
    DS::FreeSock(client.m_sock);
}

void DS::GateKeeper_Init()
{ }

void DS::GateKeeper_Add(DS::SocketHandle client)
{
#ifdef DEBUG
    if (s_commdebug)
        printf("Connecting GATE on %s\n", DS::SockIpAddress(client).c_str());
#endif

    std::thread threadh(&wk_gateKeeper, client);
    threadh.detach();
}

void DS::GateKeeper_Shutdown()
{
    {
        std::lock_guard<std::mutex> clientGuard(s_clientMutex);
        for (auto client_iter = s_clients.begin(); client_iter != s_clients.end(); ++client_iter)
            DS::CloseSock((*client_iter)->m_sock);
    }

    bool complete = false;
    for (int i=0; i<50 && !complete; ++i) {
        s_clientMutex.lock();
        size_t alive = s_clients.size();
        s_clientMutex.unlock();
        if (alive == 0)
            complete = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!complete)
        fputs("[GateKeeper] Clients didn't die after 5 seconds!\n", stderr);
}

void DS::GateKeeper_DisplayClients()
{
    std::lock_guard<std::mutex> clientGuard(s_clientMutex);
    if (s_clients.size())
        fputs("Gate Keeper:\n", stdout);
    for (auto client_iter = s_clients.begin(); client_iter != s_clients.end(); ++client_iter)
        printf("  * %s\n", DS::SockIpAddress((*client_iter)->m_sock).c_str());
}
