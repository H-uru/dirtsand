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

#include "GateServ.h"
#include "NetIO/CryptIO.h"
#include "Types/Uuid.h"
#include "settings.h"
#include "streams.h"
#include "errors.h"
#include <pthread.h>
#include <unistd.h>
#include <list>

extern bool s_commdebug;

struct GateKeeper_Private
{
    DS::SocketHandle m_sock;
    DS::CryptState m_crypt;
    DS::BufferStream m_buffer;
};

static std::list<GateKeeper_Private*> s_clients;
static pthread_mutex_t s_clientMutex;

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
    DS_PASSERT(size == 20);
    DS::Uuid uuid;
    DS::RecvBuffer(client.m_sock, uuid.m_bytes, sizeof(uuid.m_bytes));

    /* Reply header */
    client.m_buffer.truncate();
    client.m_buffer.write<uint8_t>(DS::e_ServToCliEncrypt);

    /* Establish encryption, and write reply body */
    uint8_t msgId = DS::RecvValue<uint8_t>(client.m_sock);
    DS_PASSERT(msgId == DS::e_CliToServConnect);
    uint8_t msgSize = DS::RecvValue<uint8_t>(client.m_sock);
    if (msgSize == 2) {
        // no seed... client wishes unencrypted connection (that's okay, nobody
        // else can "fake" us as nobody has the private key, so if the client
        // actually wants encryption it will only work with the correct peer)
        client.m_buffer.write<uint8_t>(2); // reply with an empty seed as well
    } else {
        uint8_t Y[64];
        DS_PASSERT(msgSize == 66);
        DS::RecvBuffer(client.m_sock, Y, 64);
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
    uint32_t payloadSize = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(payloadSize);
    if (payloadSize) {
        uint8_t* payload = new uint8_t[payloadSize];
        DS::CryptRecvBuffer(client.m_sock, client.m_crypt, payload, payloadSize);
        client.m_buffer.writeBytes(payload, payloadSize);
        delete[] payload;
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
    DS::StringBuffer<chr16_t> address = DS::Settings::FileServerAddress();
    client.m_buffer.write<uint16_t>(address.length());
    client.m_buffer.writeBytes(address.data(), address.length() * sizeof(chr16_t));

    SEND_REPLY();
}

void cb_authServIpAddress(GateKeeper_Private& client)
{
    START_REPLY(e_GateKeeperToCli_AuthServIpAddressReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    // Address
    DS::StringBuffer<chr16_t> address = DS::Settings::AuthServerAddress();
    client.m_buffer.write<uint16_t>(address.length());
    client.m_buffer.writeBytes(address.data(), address.length() * sizeof(chr16_t));

    SEND_REPLY();
}

void* wk_gateKeeper(void* sockp)
{
    GateKeeper_Private client;
    client.m_crypt = 0;

    pthread_mutex_lock(&s_clientMutex);
    client.m_sock = reinterpret_cast<DS::SocketHandle>(sockp);
    s_clients.push_back(&client);
    pthread_mutex_unlock(&s_clientMutex);

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
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[GateKeeper] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
    } catch (DS::SockHup) {
        // Socket closed...
    }

    pthread_mutex_lock(&s_clientMutex);
    auto client_iter = s_clients.begin();
    while (client_iter != s_clients.end()) {
        if (*client_iter == &client)
            client_iter = s_clients.erase(client_iter);
        else
            ++client_iter;
    }
    pthread_mutex_unlock(&s_clientMutex);

    DS::CryptStateFree(client.m_crypt);
    DS::FreeSock(client.m_sock);
    return 0;
}

void DS::GateKeeper_Init()
{
    pthread_mutex_init(&s_clientMutex, 0);
}

void DS::GateKeeper_Add(DS::SocketHandle client)
{
#ifdef DEBUG
    if (s_commdebug)
        printf("Connecting GATE on %s\n", DS::SockIpAddress(client).c_str());
#endif

    pthread_t threadh;
    pthread_create(&threadh, 0, &wk_gateKeeper, reinterpret_cast<void*>(client));
    pthread_detach(threadh);
}

void DS::GateKeeper_Shutdown()
{
    pthread_mutex_lock(&s_clientMutex);
    for (auto client_iter = s_clients.begin(); client_iter != s_clients.end(); ++client_iter)
        DS::CloseSock((*client_iter)->m_sock);
    pthread_mutex_unlock(&s_clientMutex);

    bool complete = false;
    for (int i=0; i<50 && !complete; ++i) {
        pthread_mutex_lock(&s_clientMutex);
        size_t alive = s_clients.size();
        pthread_mutex_unlock(&s_clientMutex);
        if (alive == 0)
            complete = true;
        usleep(100000);
    }
    if (!complete)
        fprintf(stderr, "[GateKeeper] Clients didn't die after 5 seconds!\n");
    pthread_mutex_destroy(&s_clientMutex);
}

void DS::GateKeeper_DisplayClients()
{
    pthread_mutex_lock(&s_clientMutex);
    if (s_clients.size())
        printf("Gate Keeper:\n");
    for (auto client_iter = s_clients.begin(); client_iter != s_clients.end(); ++client_iter)
        printf("  * %s\n", DS::SockIpAddress((*client_iter)->m_sock).c_str());
    pthread_mutex_unlock(&s_clientMutex);
}
