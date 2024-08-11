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
#include "AuthManifest.h"
#include "SDL/DescriptorDb.h"
#include "Types/BitVector.h"
#include "Types/Uuid.h"
#include "settings.h"
#include "errors.h"

#include <string_theory/format>
#include <openssl/rand.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#define NODE_SIZE_MAX (4 * 1024 * 1024)

std::list<AuthServer_Private*> s_authClients;
std::mutex s_authClientMutex;

#define START_REPLY(msgId) \
    client.m_buffer.truncate(); \
    client.m_buffer.write<uint16_t>(msgId)

#define SEND_REPLY() \
    DS::CryptSendBuffer(client.m_sock, client.m_crypt, \
                        client.m_buffer.buffer(), client.m_buffer.size())

void auth_init(AuthServer_Private& client)
{
    /* Auth server header:  size, null uuid */
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
        DS::CryptEstablish(serverSeed, sharedKey, DS::Settings::CryptKey(DS::e_KeyAuth_N),
                           DS::Settings::CryptKey(DS::e_KeyAuth_K), Y);
        client.m_crypt = DS::CryptStateInit(sharedKey, 7);

        client.m_buffer.write<uint8_t>(9);
        client.m_buffer.writeBytes(serverSeed, 7);
    }

    /* send encryption reply */
    DS::SendBuffer(client.m_sock, client.m_buffer.buffer(), client.m_buffer.size());

    /* Shard Capabilities */
    DS::BitVector caps;
    caps.set(e_CapsScoreLeaderBoards, true);

    /* BCast Shard Capabilities */
    START_REPLY(e_AuthToCli_ServerCaps);
    uint32_t bufSzPos = client.m_buffer.tell();
#ifdef DS_OU_COMPATIBLE
    // OpenUru parses this as the start of a FileDownloadChunk message,
    // which will be ignored,
    // because no file download transaction is active.
    client.m_buffer.write<uint32_t>(37);

    // H'uru sees: 0x25 bytes of data, bit vector is 1 dword long, value is [caps]
    // OpenUru sees: message type 0x25 (FileDownloadChunk), transaction ID 0x10000, error code 0x[caps]0000, file size 0x0000[caps] (continued below)
    caps.write(&client.m_buffer);

    // Ensure the caps haven't changed size and broken this compat hack
    uint32_t newBufPos = client.m_buffer.tell();
    DS_ASSERT(newBufPos - bufSzPos == 3 * sizeof(uint32_t));

    // ServerCaps message extra data, which is ignored by H'uru,
    // because the bit vector doesn't have this many dwords.
    // OpenUru parses this as the rest of FileDownloadChunk message:
    client.m_buffer.write<uint16_t>(0); // file size 0x0000[caps] (continued)
    client.m_buffer.write<uint32_t>(0); // chunk offset 0
    client.m_buffer.write<uint32_t>(19); // chunk size 19
    client.m_buffer.writeBytes("[ServerCaps compat]", 19); // 19 bytes of chunk data
#else
    client.m_buffer.write<uint32_t>(0);
    caps.write(&client.m_buffer);
    client.m_buffer.seek(bufSzPos, 0);
    client.m_buffer.write<uint32_t>(client.m_buffer.size() - bufSzPos - sizeof(uint32_t));
#endif
    SEND_REPLY();
}

void cb_ping(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_PingReply);

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

void cb_register(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ClientRegisterReply);

    // Build ID
    uint32_t buildId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    if (buildId && buildId != DS::Settings::BuildId()) {
        ST::printf(stderr, "[Auth] Wrong Build ID from {}: {}\n",
                   DS::SockIpAddress(client.m_sock), buildId);
        DS::CloseSock(client.m_sock);
        return;
    }

    // Client challenge
    RAND_bytes(reinterpret_cast<unsigned char*>(&client.m_serverChallenge),
               sizeof(client.m_serverChallenge));
    client.m_buffer.write<uint32_t>(client.m_serverChallenge);

    SEND_REPLY();
}

void cb_login(AuthServer_Private& client)
{
    Auth_LoginInfo msg;
    msg.m_client = &client;
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_clientChallenge = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_acctName = DS::CryptRecvString(client.m_sock, client.m_crypt);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt,
                        msg.m_passHash.m_data, sizeof(DS::ShaHash));
    msg.m_token = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_os = DS::CryptRecvString(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthClientLogin, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    if (reply.m_messageType != DS::e_NetSuccess) {
        static uint32_t zerokey[4] = { 0, 0, 0, 0 };

        START_REPLY(e_AuthToCli_AcctLoginReply);
        client.m_buffer.write<uint32_t>(transId);
        client.m_buffer.write<uint32_t>(reply.m_messageType);
        client.m_buffer.writeBytes(client.m_acctUuid.m_bytes, sizeof(client.m_acctUuid.m_bytes));
        client.m_buffer.write<uint32_t>(0);
        client.m_buffer.write<uint32_t>(0);
        client.m_buffer.writeBytes(zerokey, sizeof(zerokey));
        SEND_REPLY();
        return;
    }

    for (auto player_iter = msg.m_players.begin(); player_iter != msg.m_players.end(); ++player_iter) {
        START_REPLY(e_AuthToCli_AcctPlayerInfo);
        client.m_buffer.write<uint32_t>(transId);
        client.m_buffer.write<uint32_t>(player_iter->m_playerId);
        client.m_buffer.writePString<uint16_t>(player_iter->m_playerName, DS::e_StringUTF16);
        client.m_buffer.writePString<uint16_t>(player_iter->m_avatarModel, DS::e_StringUTF16);
        client.m_buffer.write<uint32_t>(player_iter->m_explorer);
        SEND_REPLY();
    }

    /* The final reply */
    START_REPLY(e_AuthToCli_AcctLoginReply);
    client.m_buffer.write<uint32_t>(transId);
    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    client.m_buffer.writeBytes(client.m_acctUuid.m_bytes, sizeof(client.m_acctUuid.m_bytes));
    client.m_buffer.write<uint32_t>(client.m_acctFlags);
    client.m_buffer.write<uint32_t>(msg.m_billingType);
    client.m_buffer.writeBytes(DS::Settings::DroidKey(), 4 * sizeof(uint32_t));
    SEND_REPLY();
}

void cb_setPlayer(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_AcctSetPlayerReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    // Player ID
    client.m_player.m_playerId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    if (client.m_player.m_playerId == 0) {
        // No player -- always successful
        client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    } else {
        Auth_ClientMessage msg;
        msg.m_client = &client;
        s_authChannel.putMessage(e_AuthSetPlayer, reinterpret_cast<void*>(&msg));
        DS::FifoMessage reply = client.m_channel.getMessage();
        client.m_buffer.write<uint32_t>(reply.m_messageType);
    }

    SEND_REPLY();
}

void cb_playerCreate(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_PlayerCreateReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_PlayerCreate msg;
    msg.m_client = &client;
    msg.m_player.m_playerName = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_player.m_avatarModel = DS::CryptRecvString(client.m_sock, client.m_crypt);
    DS::CryptRecvString(client.m_sock, client.m_crypt);   // Friend invite
    s_authChannel.putMessage(e_AuthCreatePlayer, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0);   // Player ID
        client.m_buffer.write<uint32_t>(0);   // Explorer
        client.m_buffer.write<uint16_t>(0);   // Player Name
        client.m_buffer.write<uint16_t>(0);   // Avatar Model
    } else {
        client.m_buffer.write<uint32_t>(msg.m_player.m_playerId);
        client.m_buffer.write<uint32_t>(1);   // Explorer
        client.m_buffer.writePString<uint16_t>(msg.m_player.m_playerName, DS::e_StringUTF16);
        client.m_buffer.writePString<uint16_t>(msg.m_player.m_avatarModel, DS::e_StringUTF16);
    }

    SEND_REPLY();
}

void cb_playerDelete(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_PlayerDeleteReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_PlayerDelete msg;
    msg.m_client = &client;
    msg.m_playerId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthDeletePlayer, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);

    SEND_REPLY();
}

void cb_ageCreate(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultInitAgeReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_AgeCreate msg;
    msg.m_client = &client;
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, msg.m_age.m_ageId.m_bytes,
                        sizeof(msg.m_age.m_ageId.m_bytes));
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, msg.m_age.m_parentId.m_bytes,
                        sizeof(msg.m_age.m_parentId.m_bytes));
    msg.m_age.m_filename = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_age.m_instName = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_age.m_userName = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_age.m_description = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_age.m_seqNumber = DS::CryptRecvValue<int32_t>(client.m_sock, client.m_crypt);
    msg.m_age.m_language = DS::CryptRecvValue<int32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_VaultInitAge, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0);   // Age Node Idx
        client.m_buffer.write<uint32_t>(0);   // Age Info Node Idx
    } else {
        client.m_buffer.write<uint32_t>(msg.m_ageIdx);
        client.m_buffer.write<uint32_t>(msg.m_infoIdx);
    }

    SEND_REPLY();
}

void cb_nodeCreate(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultNodeCreated);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    uint32_t nodeSize = DS::CryptRecvSize(client.m_sock, client.m_crypt, NODE_SIZE_MAX);
    std::unique_ptr<uint8_t[]> nodeBuffer(new uint8_t[nodeSize]);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, nodeBuffer.get(), nodeSize);
    DS::Blob nodeData = DS::Blob::Steal(nodeBuffer.release(), nodeSize);
    DS::BlobStream nodeStream(nodeData);

    Auth_NodeInfo msg;
    msg.m_client = &client;
    msg.m_node.read(&nodeStream);
    if (!nodeStream.atEof()) {
        ST::printf(stderr, "WARNING: Ignoring {} bytes of unread data at end of node stream\n",
                   nodeStream.size() - nodeStream.tell());
    }
    s_authChannel.putMessage(e_VaultCreateNode, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess)
        client.m_buffer.write<uint32_t>(0);
    else
        client.m_buffer.write<uint32_t>(msg.m_node.m_NodeIdx);

    SEND_REPLY();
}

void cb_nodeFetch(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultNodeFetched);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_NodeInfo msg;
    msg.m_client = &client;
    msg.m_node.set_NodeIdx(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));
    s_authChannel.putMessage(e_VaultFetchNode, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0);
    } else {
        uint32_t sizePos = client.m_buffer.tell();
        client.m_buffer.write<uint32_t>(0);
        msg.m_node.write(&client.m_buffer);
        uint32_t endPos = client.m_buffer.tell();
        client.m_buffer.seek(sizePos, SEEK_SET);
        client.m_buffer.write<uint32_t>(endPos - sizePos - sizeof(uint32_t));
    }

    SEND_REPLY();
}

void cb_nodeUpdate(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultSaveNodeReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_NodeInfo msg;
    msg.m_client = &client;
    uint32_t m_nodeId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, &msg.m_revision.m_bytes,
                        sizeof(msg.m_revision.m_bytes));

    uint32_t nodeSize = DS::CryptRecvSize(client.m_sock, client.m_crypt, NODE_SIZE_MAX);
    std::unique_ptr<uint8_t[]> nodeBuffer(new uint8_t[nodeSize]);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, nodeBuffer.get(), nodeSize);
    DS::Blob nodeData = DS::Blob::Steal(nodeBuffer.release(), nodeSize);
    DS::BlobStream nodeStream(nodeData);

    msg.m_node.read(&nodeStream);
    if (!nodeStream.atEof()) {
        ST::printf(stderr, "WARNING: Ignoring {} bytes of unread data at end of node stream\n",
                   nodeStream.size() - nodeStream.tell());
    }
    msg.m_node.m_NodeIdx = m_nodeId;
    msg.m_internal = false;
    s_authChannel.putMessage(e_VaultUpdateNode, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);

    SEND_REPLY();
}

void cb_nodeRef(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultAddNodeReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_NodeRef msg;
    msg.m_client = &client;
    msg.m_ref.m_parent = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_ref.m_child = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_ref.m_owner = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_VaultRefNode, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);

    SEND_REPLY();
}

void cb_nodeUnref(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultAddNodeReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_NodeRef msg;
    msg.m_client = &client;
    msg.m_ref.m_parent = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_ref.m_child = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_VaultUnrefNode, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);

    SEND_REPLY();
}

void cb_nodeTree(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultNodeRefsFetched);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_NodeRefList msg;
    msg.m_client = &client;
    msg.m_nodeId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_VaultFetchNodeTree, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0);
    } else {
        client.m_buffer.write<uint32_t>(msg.m_refs.size());
        for (auto it = msg.m_refs.begin(); it != msg.m_refs.end(); ++it) {
            client.m_buffer.write<uint32_t>(it->m_parent);
            client.m_buffer.write<uint32_t>(it->m_child);
            client.m_buffer.write<uint32_t>(it->m_owner);
            client.m_buffer.write<uint8_t>(0);
        }
    }

    SEND_REPLY();
}

void cb_nodeFind(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_VaultNodeFindReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_NodeFindList msg;
    msg.m_client = &client;

    uint32_t nodeSize = DS::CryptRecvSize(client.m_sock, client.m_crypt, NODE_SIZE_MAX);
    std::unique_ptr<uint8_t[]> nodeBuffer(new uint8_t[nodeSize]);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, nodeBuffer.get(), nodeSize);
    DS::Blob nodeData = DS::Blob::Steal(nodeBuffer.release(), nodeSize);
    DS::BlobStream nodeStream(nodeData);

    msg.m_template.read(&nodeStream);
    if (!nodeStream.atEof()) {
        ST::printf(stderr, "WARNING: Ignoring {} bytes of unread data at end of node stream\n",
                   nodeStream.size() - nodeStream.tell());
    }
    s_authChannel.putMessage(e_VaultFindNode, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0);
    } else {
        client.m_buffer.write<uint32_t>(msg.m_nodes.size());
        for (size_t i=0; i<msg.m_nodes.size(); ++i)
            client.m_buffer.write<uint32_t>(msg.m_nodes[i]);
    }

    SEND_REPLY();
}

void cb_nodeSend(AuthServer_Private& client)
{
    Auth_NodeSend msg;
    msg.m_client = &client;
    msg.m_senderIdx = client.m_player.m_playerId;
    msg.m_nodeIdx = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_playerIdx = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_VaultSendNode, reinterpret_cast<void*>(&msg));
    client.m_channel.getMessage(); // wait for the vault operation to complete before returning
}

void cb_ageRequest(AuthServer_Private& client, bool ext)
{
    START_REPLY(ext ? e_AuthToCli_AgeReplyEx : e_AuthToCli_AgeReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    Auth_GameAge msg;
    msg.m_client = &client;
    msg.m_name = DS::CryptRecvString(client.m_sock, client.m_crypt);
    DS::CryptRecvBuffer(client.m_sock, client.m_crypt, &msg.m_instanceId.m_bytes,
                        sizeof(msg.m_instanceId.m_bytes));
    s_authChannel.putMessage(e_AuthFindGameServer, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0);   // MCP ID
        client.m_buffer.write<DS::Uuid>(DS::Uuid());
        client.m_buffer.write<uint32_t>(0);   // Age Node Idx
        // Game server address
        if (ext)
            client.m_buffer.write<uint16_t>(0);
        else
            client.m_buffer.write<uint32_t>(0);
    } else {
        client.m_buffer.write<uint32_t>(msg.m_mcpId);
        client.m_buffer.write<DS::Uuid>(msg.m_instanceId);
        client.m_buffer.write<uint32_t>(msg.m_ageNodeIdx);
        if (ext)
            client.m_buffer.writePString<uint16_t>(DS::Settings::GameServerAddress(), DS::e_StringUTF16);
        else
            client.m_buffer.write<uint32_t>(msg.m_serverAddress);
    }

    SEND_REPLY();
}

void cb_fileList(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_FileListReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    ST::string directory = DS::CryptRecvString(client.m_sock, client.m_crypt);
    ST::string fileext = DS::CryptRecvString(client.m_sock, client.m_crypt);

    // Manifest may not have any path characters
    if (directory.find(".") != -1 || directory.find("/") != -1
        || directory.find("\\") != -1 || directory.find(":") != -1
        || fileext.find(".") != -1 || fileext.find("/") != -1
        || fileext.find("\\") != -1 || fileext.find(":") != -1) {
        ST::printf(stderr, "[Auth] Invalid manifest request from {}: {}\\{}\n",
                   DS::SockIpAddress(client.m_sock), directory, fileext);
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }
    ST::string mfsname = ST::format("{}{}_{}.list", DS::Settings::AuthRoot(),
                                    directory, fileext);
    DS::AuthManifest mfs;
    DS::NetResultCode result = DS::e_NetPending;

    // Special case: SDL files
    // For production shards, we expect for them to be listed in the secure preloader manifest.
    // If that hasn't been done, don't worry about the SDL lists - just use the SDL files that
    // DS would load on start up.
    if (directory.compare_i("SDL") == 0 && fileext.compare_i("sdl") == 0) {
        auto populateSdl = [&mfs](const ST::string& path) {
            struct stat sbuf;
            if (stat(path.c_str(), &sbuf) < 0)
                throw DS::SystemError("[Auth] Unable to stat SDL file", strerror(errno));
            ST::string filename = path.after_last('/');
            mfs.addFile(ST::format("SDL\\{}", filename), sbuf.st_size);
            return true;
        };
        try {
            SDL::DescriptorDb::ForDescriptorFiles(DS::Settings::SdlPath(), std::move(populateSdl));
            result = DS::e_NetSuccess;
        } catch (const DS::SystemError& err) {
            fputs(err.what(), stderr);
            result = DS::e_NetInternalError;
        }
    } else {
        result = mfs.loadManifest(mfsname.c_str());
    }

    DS_ASSERT(result != DS::e_NetPending);
    client.m_buffer.write<uint32_t>(result);

    if (result != DS::e_NetSuccess) {
        ST::printf(stderr, "[Auth] {} requested invalid manifest {}\n",
                   DS::SockIpAddress(client.m_sock), mfsname);
        client.m_buffer.write<uint32_t>(0);     // Data packet size
    } else {
        uint32_t sizeLocation = client.m_buffer.tell();
        client.m_buffer.write<uint32_t>(0);
        uint32_t dataSize = mfs.encodeToStream(&client.m_buffer);
        client.m_buffer.seek(sizeLocation, SEEK_SET);
        client.m_buffer.write<uint32_t>(dataSize);
    }

    SEND_REPLY();
}

void cb_downloadStart(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_FileDownloadChunk);

    // Trans ID
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    // Download filename
    ST::string filename = DS::CryptRecvString(client.m_sock, client.m_crypt);
    filename = filename.replace("\\", "/");

    // Ensure filename is jailed to our data path
    if (filename.find("..") != -1) {
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Chunk offset
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }

    // Special case: SDL files from the server' SDL directory.
    ST_ssize_t slashPos = filename.find_last('/');
    if (slashPos != -1 && filename.left(slashPos).compare_i("SDL") == 0 && filename.after_last('.').compare_i("sdl") == 0) {
        filename = DS::Settings::SdlPath() + filename.substr(slashPos);
    } else {
        filename = DS::Settings::AuthRoot() + filename;
    }

    auto fileStream = std::make_unique<DS::FileStream>();
    try {
        fileStream->open(filename.c_str(), "rb");
    } catch (const DS::FileIOException& ex) {
        ST::printf(stderr, "[Auth] Could not open file {}: {}\n[Auth] Requested by {}\n",
                   filename, ex.what(), DS::SockIpAddress(client.m_sock));
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Chunk offset
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }

    // All auth downloads must be encrypted.
    std::unique_ptr<DS::Stream> stream;
    if (!DS::EncryptedStream::CheckEncryption(fileStream.get()).has_value()) {
        auto bufStream = std::make_unique<DS::BufferStream>();
        {
            DS::EncryptedStream encStream(bufStream.get(), DS::EncryptedStream::Mode::e_write,
                                          DS::EncryptedStream::Type::e_xxtea,
                                          DS::Settings::DroidKey());
            uint8_t buf[CHUNK_SIZE];
            while (fileStream->tell() < fileStream->size()) {
                ssize_t nread = fileStream->readBytes(buf, sizeof(buf));
                DS_ASSERT(nread >= 0);
                encStream.writeBytes(buf, nread);
            }
        }
        bufStream->seek(0, SEEK_SET);
        stream = std::move(bufStream);
    } else {
        stream = std::move(fileStream);
    }

    DS_ASSERT(stream);
    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    client.m_buffer.write<uint32_t>(stream->size());
    client.m_buffer.write<uint32_t>(stream->tell());

    uint8_t data[CHUNK_SIZE];
    if (stream->size() > CHUNK_SIZE) {
        client.m_buffer.write<uint32_t>(CHUNK_SIZE);
        stream->readBytes(data, CHUNK_SIZE);
        client.m_buffer.writeBytes(data, CHUNK_SIZE);
        client.m_downloads[transId] = std::move(stream);
    } else {
        client.m_buffer.write<uint32_t>(stream->size());
        stream->readBytes(data, stream->size());
        client.m_buffer.writeBytes(data, stream->size());
    }

    SEND_REPLY();
}

void cb_downloadNext(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_FileDownloadChunk);

    // Trans ID
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    auto fi = client.m_downloads.find(transId);
    if (fi == client.m_downloads.end()) {
        // The last chunk was already sent, we don't care anymore
        return;
    }

    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    client.m_buffer.write<uint32_t>(fi->second->size());
    client.m_buffer.write<uint32_t>(fi->second->tell());

    uint8_t data[CHUNK_SIZE];
    size_t bytesLeft = fi->second->size() - fi->second->tell();
    if (bytesLeft > CHUNK_SIZE) {
        client.m_buffer.write<uint32_t>(CHUNK_SIZE);
        fi->second->readBytes(data, CHUNK_SIZE);
        client.m_buffer.writeBytes(data, CHUNK_SIZE);
    } else {
        client.m_buffer.write<uint32_t>(bytesLeft);
        fi->second->readBytes(data, bytesLeft);
        client.m_buffer.writeBytes(data, bytesLeft);
        client.m_downloads.erase(fi);
    }

    SEND_REPLY();
}

void cb_scoreCreate(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ScoreCreateReply);

    // Trans ID
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    Auth_CreateScore msg;
    msg.m_client = &client;
    msg.m_owner = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_name = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_type = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_points = DS::CryptRecvValue<int32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthCreateScore, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0); // Score ID
        client.m_buffer.write<uint32_t>(0); // Create Time
    } else {
        client.m_buffer.write<uint32_t>(msg.m_scoreId);
        client.m_buffer.write<uint32_t>((uint32_t)time(nullptr)); // close enough.
    }
    SEND_REPLY();
}

void write_scoreBuffer(AuthServer_Private& client, const Auth_GetScores& msg,
                       const DS::FifoMessage& reply)
{
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0); // Score Count
        client.m_buffer.write<uint32_t>(0); // Buffer Size
    } else {
        client.m_buffer.write<uint32_t>(msg.m_scores.size());
        // eap sucks -- need utf16 string length in bytes
        ST::utf16_buffer name = msg.m_name.to_utf16();
        uint32_t bufsz = msg.m_scores.size() *
                        (Auth_GetScores::GameScore::BaseStride +
                        ((name.size() + 1) * sizeof(char16_t)));
        client.m_buffer.write<uint32_t>(bufsz);
        for (auto& score : msg.m_scores) {
            client.m_buffer.write<uint32_t>(score.m_scoreId);
            client.m_buffer.write<uint32_t>(score.m_owner);
            client.m_buffer.write<uint32_t>(score.m_createTime);
            client.m_buffer.write<uint32_t>(score.m_type);
            client.m_buffer.write<uint32_t>(score.m_points);
            // evil string shit
            client.m_buffer.write<uint32_t>((name.size() + 1) * sizeof(char16_t));
            client.m_buffer.writeBytes(name.data(), name.size() * sizeof(char16_t));
            client.m_buffer.write<char16_t>(0);
        }
    }
}

void cb_scoreGetScores(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ScoreGetScoresReply);
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    Auth_GetScores msg;
    msg.m_client = &client;
    msg.m_owner = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_name = DS::CryptRecvString(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthGetScores, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    write_scoreBuffer(client, msg, reply);
    SEND_REPLY();
}

void cb_scoreAddPoints(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ScoreAddPointsReply);
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    Auth_UpdateScore msg;
    msg.m_client = &client;
    msg.m_scoreId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_points = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthAddScorePoints, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    SEND_REPLY();
}

void cb_scoreTransferPoints(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ScoreAddPointsReply);
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    Auth_TransferScore msg;
    msg.m_client = &client;
    msg.m_srcScoreId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_dstScoreId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_points = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthTransferScorePoints, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    SEND_REPLY();
}

void cb_scoreSetPoints(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ScoreSetPointsReply);
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    Auth_UpdateScore msg;
    msg.m_client = &client;
    msg.m_scoreId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_points = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthSetScorePoints, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    SEND_REPLY();
}

void cb_scoreGetHighScores(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ScoreGetHighScoresReply);
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    Auth_GetHighScores msg;
    msg.m_client = &client;
    msg.m_owner = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_maxScores = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_name = DS::CryptRecvString(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthGetHighScores, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    write_scoreBuffer(client, msg, reply);
    SEND_REPLY();
}

void cb_getPublicAges(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_PublicAgeList);

    // Trans ID
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    Auth_PubAgeRequest msg;
    msg.m_client = &client;
    msg.m_agename = DS::CryptRecvString(client.m_sock, client.m_crypt);
    s_authChannel.putMessage(e_AuthGetPublic, reinterpret_cast<void*>(&msg));

    DS::FifoMessage reply = client.m_channel.getMessage();
    client.m_buffer.write<uint32_t>(reply.m_messageType);
    if (reply.m_messageType != DS::e_NetSuccess) {
        client.m_buffer.write<uint32_t>(0);
    } else {
        client.m_buffer.write<uint32_t>(msg.m_ages.size());
        for (size_t i = 0; i < msg.m_ages.size(); i++) {
            client.m_buffer.writeBytes(msg.m_ages[i].m_instance.m_bytes, sizeof(client.m_acctUuid.m_bytes));

            char16_t strbuffer[2048];
            ST::utf16_buffer buf;
            uint32_t copylen;

            buf = msg.m_agename.to_utf16();
            copylen = buf.size() < 64 ? buf.size() : 63;
            memcpy(strbuffer, buf.data(), copylen * sizeof(char16_t));
            strbuffer[copylen] = 0;
            client.m_buffer.writeBytes(strbuffer, 64 * sizeof(char16_t));

            buf = msg.m_ages[i].m_instancename.to_utf16();
            copylen = buf.size() < 64 ? buf.size() : 63;
            memcpy(strbuffer, buf.data(), copylen * sizeof(char16_t));
            strbuffer[copylen] = 0;
            client.m_buffer.writeBytes(strbuffer, 64 * sizeof(char16_t));

            buf = msg.m_ages[i].m_username.to_utf16();
            copylen = buf.size() < 64 ? buf.size() : 63;
            memcpy(strbuffer, buf.data(), copylen * sizeof(char16_t));
            strbuffer[copylen] = 0;
            client.m_buffer.writeBytes(strbuffer, 64 * sizeof(char16_t));

            buf = msg.m_ages[i].m_description.to_utf16();
            copylen = buf.size() < 1024 ? buf.size() : 1023;
            memcpy(strbuffer, buf.data(), copylen * sizeof(char16_t));
            strbuffer[copylen] = 0;
            client.m_buffer.writeBytes(strbuffer, 1024 * sizeof(char16_t));

            client.m_buffer.write<uint32_t>(msg.m_ages[i].m_sequence);
            client.m_buffer.write<uint32_t>(msg.m_ages[i].m_language);
            client.m_buffer.write<uint32_t>(msg.m_ages[i].m_population);
            client.m_buffer.write<uint32_t>(msg.m_ages[i].m_curPopulation);
        }
    }

    SEND_REPLY();
}

void cb_setAgePublic(AuthServer_Private& client)
{
    Auth_SetPublic msg;
    msg.m_client = &client;
    msg.m_node = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    msg.m_public = DS::CryptRecvValue<uint8_t>(client.m_sock, client.m_crypt);

    s_authChannel.putMessage(e_AuthSetPublic, reinterpret_cast<void*>(&msg));

    client.m_channel.getMessage(); // wait for daemon to finish
}

void cb_sockRead(AuthServer_Private& client)
{
    uint16_t msgId = DS::CryptRecvValue<uint16_t>(client.m_sock, client.m_crypt);
    switch (msgId) {
    case e_CliToAuth_PingRequest:
        cb_ping(client);
        break;
    case e_CliToAuth_ClientRegisterRequest:
        cb_register(client);
        break;
    case e_CliToAuth_AcctLoginRequest:
        cb_login(client);
        break;
    case e_CliToAuth_AcctSetPlayerRequest:
        cb_setPlayer(client);
        break;
    case e_CliToAuth_PlayerCreateRequest:
        cb_playerCreate(client);
        break;
    case e_CliToAuth_PlayerDeleteRequest:
        cb_playerDelete(client);
        break;
    case e_CliToAuth_VaultNodeCreate:
        cb_nodeCreate(client);
        break;
    case e_CliToAuth_VaultNodeFetch:
        cb_nodeFetch(client);
        break;
    case e_CliToAuth_VaultNodeSave:
        cb_nodeUpdate(client);
        break;
    case e_CliToAuth_VaultNodeAdd:
        cb_nodeRef(client);
        break;
    case e_CliToAuth_VaultNodeRemove:
        cb_nodeUnref(client);
        break;
    case e_CliToAuth_VaultFetchNodeRefs:
        cb_nodeTree(client);
        break;
    case e_CliToAuth_VaultInitAgeRequest:
        cb_ageCreate(client);
        break;
    case e_CliToAuth_VaultNodeFind:
        cb_nodeFind(client);
        break;
    case e_CliToAuth_VaultSendNode:
        cb_nodeSend(client);
        break;
    case e_CliToAuth_AgeRequest:
        cb_ageRequest(client, false);
        break;
    case e_CliToAuth_AgeRequestEx:
        cb_ageRequest(client, true);
        break;
    case e_CliToAuth_FileListRequest:
        cb_fileList(client);
        break;
    case e_CliToAuth_FileDownloadRequest:
        cb_downloadStart(client);
        break;
    case e_CliToAuth_FileDownloadChunkAck:
        cb_downloadNext(client);
        break;
    case e_CliToAuth_LogPythonTraceback:
        ST::printf("[Auth] Got client python traceback:\n{}\n",
                   DS::CryptRecvString(client.m_sock, client.m_crypt));
        break;
    case e_CliToAuth_LogStackDump:
        ST::printf("[Auth] Got client stackdump:\n{}\n",
                   DS::CryptRecvString(client.m_sock, client.m_crypt));
        break;
    case e_CliToAuth_LogClientDebuggerConnect:
        // Nobody cares
        break;
    case e_CliToAuth_ScoreCreate:
        cb_scoreCreate(client);
        break;
    case e_CliToAuth_ScoreGetScores:
        cb_scoreGetScores(client);
        break;
    case e_CliToAuth_ScoreAddPoints:
        cb_scoreAddPoints(client);
        break;
    case e_CliToAuth_ScoreTransferPoints:
        cb_scoreTransferPoints(client);
        break;
    case e_CliToAuth_ScoreSetPoints:
        cb_scoreSetPoints(client);
        break;
    case e_CliToAuth_ScoreGetHighScores:
        cb_scoreGetHighScores(client);
        break;
    case e_CliToAuth_GetPublicAgeList:
        cb_getPublicAges(client);
        break;
    case e_CliToAuth_SetAgePublic:
        cb_setAgePublic(client);
        break;
    case e_CliToAuth_ClientSetCCRLevel:
    case e_CliToAuth_AcctSetRolesRequest:
    case e_CliToAuth_AcctSetBillingTypeRequest:
    case e_CliToAuth_AcctActivateRequest:
    case e_CliToAuth_AcctCreateFromKeyRequest:
    case e_CliToAuth_VaultNodeDelete:
    case e_CliToAuth_UpgradeVisitorRequest:
    case e_CliToAuth_SetPlayerBanStatusRequest:
    case e_CliToAuth_KickPlayer:
        ST::printf(stderr, "[Auth] Got unsupported client message {} from {}\n",
                   msgId, DS::SockIpAddress(client.m_sock));
        DS::CloseSock(client.m_sock);
        throw DS::SockHup();
    default:
        /* Invalid message */
        ST::printf(stderr, "[Auth] Got invalid message ID {} from {}\n",
                   msgId, DS::SockIpAddress(client.m_sock));
        DS::CloseSock(client.m_sock);
        throw DS::SockHup();
    }
}

void cb_broadcast(AuthServer_Private& client)
{
    DS::FifoMessage bcast = client.m_broadcast.getMessage();
    DS::BufferStream* msg = reinterpret_cast<DS::BufferStream*>(bcast.m_payload);
    START_REPLY(bcast.m_messageType);
    client.m_buffer.writeBytes(msg->buffer(), msg->size());
    msg->unref();
    SEND_REPLY();
}

void wk_authWorker(DS::SocketHandle sockp)
{
    AuthServer_Private client;
    client.m_crypt = nullptr;
    client.m_sock = sockp;

    try {
        auth_init(client);
        client.m_player.m_playerId = 0;

        // Now that we're encrypted, we can add the client to our list
        s_authClientMutex.lock();
        s_authClients.push_back(&client);
        s_authClientMutex.unlock();

        // Poll the client socket and the daemon broadcast channel for messages
        pollfd fds[2];
        fds[0].fd = DS::SockFd(sockp);
        fds[0].events = POLLIN;
        fds[1].fd = client.m_broadcast.fd();
        fds[1].events = POLLIN;

        for ( ;; ) {
            int result = poll(fds, 2, NET_TIMEOUT * 1000);
            if (result < 0) {
                ST::printf(stderr, "[Auth] Failure in poll: {}\n", strerror(errno));
                throw DS::SockHup();
            }
            if (result == 0 || fds[0].revents & (POLLERR | POLLHUP | POLLNVAL) || fds[1].revents & (POLLERR | POLLNVAL))
                throw DS::SockHup();

            if (fds[0].revents & POLLIN)
                cb_sockRead(client);
            if (fds[1].revents & POLLIN)
                cb_broadcast(client);
        }
    } catch (const DS::SockHup&) {
        // Socket closed...
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] Error processing client message from {}: {}\n",
                   DS::SockIpAddress(sockp), ex.what());
    }

    Auth_ClientMessage disconMsg;
    disconMsg.m_client = &client;
    try {
        s_authChannel.putMessage(e_AuthDisconnect, reinterpret_cast<void*>(&disconMsg));
        client.m_channel.getMessage();
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }

    s_authClientMutex.lock();
    s_authClients.remove(&client);
    s_authClientMutex.unlock();

    // Drain the broadcast channel
    try {
        while (client.m_broadcast.hasMessage()) {
            DS::FifoMessage msg = client.m_broadcast.getMessage();
            reinterpret_cast<DS::BufferStream*>(msg.m_payload)->unref();
        }
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }

    DS::CryptStateFree(client.m_crypt);
    DS::FreeSock(client.m_sock);
}

void DS::AuthServer_Init(bool restrictLogins)
{
    s_authDaemonThread = std::thread(&dm_authDaemon);
    if (restrictLogins) {
        try {
            s_authChannel.putMessage(e_AuthRestrictLogins);
        } catch (const std::exception& ex) {
            ST::printf(stderr, "Warning: Could not restrict logins: {}\n", ex.what());
        }
    }
}

void DS::AuthServer_Add(DS::SocketHandle client)
{
#ifdef DEBUG
    if (s_commdebug)
        ST::printf("Connecting AUTH on {}\n", DS::SockIpAddress(client));
#endif

    std::thread threadh(&wk_authWorker, client);
    threadh.detach();
}

bool DS::AuthServer_RestrictLogins()
{
    AuthClient_Private client;
    Auth_RestrictLogins msg;
    msg.m_client = &client;
    try {
        s_authChannel.putMessage(e_AuthRestrictLogins, &msg);
        client.m_channel.getMessage();
        return msg.m_status;
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }
    return false;
}

void DS::AuthServer_Shutdown()
{
    try {
        s_authChannel.putMessage(e_AuthShutdown);
    } catch (const std::exception& ex) {
        // NOTE: This will probably cause the join to hang
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }
    s_authDaemonThread.join();
}

void DS::AuthServer_DisplayClients()
{
    std::lock_guard<std::mutex> authClientGuard(s_authClientMutex);
    if (s_authClients.size())
        fputs("Auth Server:\n", stdout);
    for (const auto& client : s_authClients) {
        ST::printf("  * {} {}\n", DS::SockIpAddress(client->m_sock),
                   client->m_acctUuid.toString(true));
    }
}

bool DS::AuthServer_AddAcct(const ST::string& acctName, const ST::string& password)
{
    AuthClient_Private client;
    Auth_AddAcct req;
    req.m_client = &client;
    req.m_acctInfo.m_acctName = acctName;
    req.m_acctInfo.m_password = password;
    try {
        s_authChannel.putMessage(e_AuthAddAcct, &req);
        DS::FifoMessage reply = client.m_channel.getMessage();
        return reply.m_messageType == DS::e_NetSuccess;
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }
    return false;
}

uint32_t DS::AuthServer_AcctFlags(const ST::string& acctName, uint32_t flags)
{
    AuthClient_Private client;
    Auth_AccountFlags req;
    req.m_client = &client;
    req.m_acctName = acctName;
    req.m_flags = flags;
    try {
        s_authChannel.putMessage(e_AuthAcctFlags, &req);
        DS::FifoMessage reply = client.m_channel.getMessage();
        if (reply.m_messageType == DS::e_NetSuccess)
            return req.m_flags;
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }
    return static_cast<uint32_t>(-1);
}

bool DS::AuthServer_AddAllPlayersFolder(uint32_t playerId)
{
    AuthClient_Private client;
    Auth_AddAllPlayers msg;
    msg.m_client = &client;
    msg.m_playerId = playerId;
    try {
        s_authChannel.putMessage(e_AuthAddAllPlayers, &msg);
        return (client.m_channel.getMessage().m_messageType == DS::e_NetSuccess);
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }
    return false;
}

bool DS::AuthServer_ChangeGlobalSDL(const ST::string& ageName, const ST::string& var, const ST::string& value)
{
    AuthClient_Private client;
    Auth_UpdateGlobalSDL msg;
    msg.m_client = &client;
    msg.m_ageFilename = ageName;
    msg.m_variable = var;
    msg.m_value = value;
    try {
        s_authChannel.putMessage(e_AuthUpdateGlobalSDL, &msg);
        return (client.m_channel.getMessage().m_messageType == DS::e_NetSuccess);
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[Auth] WARNING: {}\n", ex.what());
    }
    return false;
}
