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
#include "AuthManifest.h"
#include "Types/Uuid.h"
#include "settings.h"
#include "errors.h"
#include <openssl/rand.h>

extern bool s_commdebug;
bool s_authServerRunning = false;

std::list<AuthServer_Private*> s_authClients;
pthread_mutex_t s_authClientMutex;

enum AuthServer_MsgIds
{
    e_CliToAuth_PingRequest = 0, e_CliToAuth_ClientRegisterRequest,
    e_CliToAuth_ClientSetCCRLevel, e_CliToAuth_AcctLoginRequest,
    e_CliToAuth_AcctSetEulaVersion, e_CliToAuth_AcctSetDataRequest,
    e_CliToAuth_AcctSetPlayerRequest, e_CliToAuth_AcctCreateRequest,
    e_CliToAuth_AcctChangePasswordRequest, e_CliToAuth_AcctSetRolesRequest,
    e_CliToAuth_AcctSetBillingTypeRequest, e_CliToAuth_AcctActivateRequest,
    e_CliToAuth_AcctCreateFromKeyRequest, e_CliToAuth_PlayerDeleteRequest,
    e_CliToAuth_PlayerUndeleteRequest, e_CliToAuth_PlayerSelectRequest,
    e_CliToAuth_PlayerRenameRequest, e_CliToAuth_PlayerCreateRequest,
    e_CliToAuth_PlayerSetStatus, e_CliToAuth_PlayerChat,
    e_CliToAuth_UpgradeVisitorRequest, e_CliToAuth_SetPlayerBanStatusRequest,
    e_CliToAuth_KickPlayer, e_CliToAuth_ChangePlayerNameRequest,
    e_CliToAuth_SendFriendInviteRequest, e_CliToAuth_VaultNodeCreate,
    e_CliToAuth_VaultNodeFetch, e_CliToAuth_VaultNodeSave,
    e_CliToAuth_VaultNodeDelete, e_CliToAuth_VaultNodeAdd,
    e_CliToAuth_VaultNodeRemove, e_CliToAuth_VaultFetchNodeRefs,
    e_CliToAuth_VaultInitAgeRequest, e_CliToAuth_VaultNodeFind,
    e_CliToAuth_VaultSetSeen, e_CliToAuth_VaultSendNode, e_CliToAuth_AgeRequest,
    e_CliToAuth_FileListRequest, e_CliToAuth_FileDownloadRequest,
    e_CliToAuth_FileDownloadChunkAck, e_CliToAuth_PropagateBuffer,
    e_CliToAuth_GetPublicAgeList, e_CliToAuth_SetAgePublic,
    e_CliToAuth_LogPythonTraceback, e_CliToAuth_LogStackDump,
    e_CliToAuth_LogClientDebuggerConnect, e_CliToAuth_ScoreCreate,
    e_CliToAuth_ScoreDelete, e_CliToAuth_ScoreGetScores,
    e_CliToAuth_ScoreAddPoints, e_CliToAuth_ScoreTransferPoints,
    e_CliToAuth_ScoreSetPoints, e_CliToAuth_ScoreGetRanks,
    e_CliToAuth_AcctExistsRequest,

    e_AuthToCli_PingReply = 0, kAuthToCli_ServerAddr, e_AuthToCli_NotifyNewBuild,
    e_AuthToCli_ClientRegisterReply, e_AuthToCli_AcctLoginReply,
    e_AuthToCli_AcctData, e_AuthToCli_AcctPlayerInfo,
    e_AuthToCli_AcctSetPlayerReply, e_AuthToCli_AcctCreateReply,
    e_AuthToCli_AcctChangePasswordReply, e_AuthToCli_AcctSetRolesReply,
    e_AuthToCli_AcctSetBillingTypeReply, e_AuthToCli_AcctActivateReply,
    e_AuthToCli_AcctCreateFromKeyReply, e_AuthToCli_PlayerList,
    e_AuthToCli_PlayerChat, e_AuthToCli_PlayerCreateReply,
    e_AuthToCli_PlayerDeleteReply, e_AuthToCli_UpgradeVisitorReply,
    e_AuthToCli_SetPlayerBanStatusReply, e_AuthToCli_ChangePlayerNameReply,
    e_AuthToCli_SendFriendInviteReply, e_AuthToCli_FriendNotify,
    e_AuthToCli_VaultNodeCreated, e_AuthToCli_VaultNodeFetched,
    e_AuthToCli_VaultNodeChanged, e_AuthToCli_VaultNodeDeleted,
    e_AuthToCli_VaultNodeAdded, e_AuthToCli_VaultNodeRemoved,
    e_AuthToCli_VaultNodeRefsFetched, e_AuthToCli_VaultInitAgeReply,
    e_AuthToCli_VaultNodeFindReply, e_AuthToCli_VaultSaveNodeReply,
    e_AuthToCli_VaultAddNodeReply, e_AuthToCli_VaultRemoveNodeReply,
    e_AuthToCli_AgeReply, e_AuthToCli_FileListReply,
    e_AuthToCli_FileDownloadChunk, e_AuthToCli_PropagateBuffer,
    e_AuthToCli_KickedOff, e_AuthToCli_PublicAgeList,
    e_AuthToCli_ScoreCreateReply, e_AuthToCli_ScoreDeleteReply,
    e_AuthToCli_ScoreGetScoresReply, e_AuthToCli_ScoreAddPointsReply,
    e_AuthToCli_ScoreTransferPointsReply, e_AuthToCli_ScoreSetPointsReply,
    e_AuthToCli_ScoreGetRanksReply, e_AuthToCli_AcctExistsReply,
};

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
    DS_PASSERT(size == 20);
    DS::Uuid uuid;
    DS::RecvBuffer(client.m_sock, uuid.m_bytes, sizeof(uuid.m_bytes));

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
    DS::CryptEstablish(serverSeed, sharedKey, DS::Settings::CryptKey(DS::e_KeyAuth_N),
                       DS::Settings::CryptKey(DS::e_KeyAuth_K), Y);

    client.m_buffer.truncate();
    client.m_buffer.write<uint8_t>(DS::e_ServToCliEncrypt);
    client.m_buffer.write<uint8_t>(9);
    client.m_buffer.writeBytes(serverSeed, 7);
    DS::SendBuffer(client.m_sock, client.m_buffer.buffer(), client.m_buffer.size());

    client.m_crypt = DS::CryptStateInit(sharedKey, 7);
}

void cb_ping(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_PingReply);

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

void cb_register(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_ClientRegisterReply);

    // Build ID
    uint32_t buildId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    if (buildId && buildId != CLIENT_BUILD_ID) {
        fprintf(stderr, "[Auth] Wrong Build ID from %s: %d\n",
                DS::SockIpAddress(client.m_sock).c_str(), buildId);
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

    for (std::vector<AuthServer_PlayerInfo>::iterator player_iter = msg.m_players.begin();
         player_iter != msg.m_players.end(); ++player_iter) {
        START_REPLY(e_AuthToCli_AcctLoginReply);
        client.m_buffer.write<uint32_t>(transId);
        client.m_buffer.write<uint32_t>(player_iter->m_playerId);
        DS::StringBuffer<chr16_t> wstrbuf = player_iter->m_playerName.toUtf16();
        client.m_buffer.write<uint16_t>(wstrbuf.length());
        client.m_buffer.writeBytes(wstrbuf.data(), wstrbuf.length() * sizeof(chr16_t));
        wstrbuf = player_iter->m_avatarModel.toUtf16();
        client.m_buffer.write<uint16_t>(wstrbuf.length());
        client.m_buffer.writeBytes(wstrbuf.data(), wstrbuf.length() * sizeof(chr16_t));
        client.m_buffer.write<uint32_t>(player_iter->m_explorer);
        SEND_REPLY();
    }

    /* The final reply */
    START_REPLY(e_AuthToCli_AcctLoginReply);
    client.m_buffer.write<uint32_t>(transId);
    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    client.m_buffer.writeBytes(client.m_acctUuid.m_bytes, sizeof(client.m_acctUuid.m_bytes));
    client.m_buffer.write<uint32_t>(msg.m_acctFlags);
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
    msg.m_playerName = DS::CryptRecvString(client.m_sock, client.m_crypt);
    msg.m_avatarShape = DS::CryptRecvString(client.m_sock, client.m_crypt);
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
        client.m_buffer.write<uint32_t>(msg.m_playerNode);
        client.m_buffer.write<uint32_t>(1);   // Explorer
        DS::StringBuffer<chr16_t> wbuf = msg.m_playerName.toUtf16();
        client.m_buffer.write<uint16_t>(wbuf.length());
        client.m_buffer.writeBytes(wbuf.data(), wbuf.length() * sizeof(chr16_t));
        wbuf = msg.m_avatarShape.toUtf16();
        client.m_buffer.write<uint16_t>(wbuf.length());
        client.m_buffer.writeBytes(wbuf.data(), wbuf.length() * sizeof(chr16_t));
    }

    SEND_REPLY();
}

void cb_fileList(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_FileListReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt));

    DS::String directory = DS::CryptRecvString(client.m_sock, client.m_crypt);
    DS::String fileext = DS::CryptRecvString(client.m_sock, client.m_crypt);

    // Manifest may not have any path characters
    if (directory.find(".") != -1 || directory.find("/") != -1
        || directory.find("\\") != -1 || directory.find(":") != -1
        || fileext.find(".") != -1 || fileext.find("/") != -1
        || fileext.find("\\") != -1 || fileext.find(":") != -1) {
        fprintf(stderr, "[Auth] Invalid manifest request from %s: %s\\%s\n",
                DS::SockIpAddress(client.m_sock).c_str(), directory.c_str(),
                fileext.c_str());
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }
    DS::String mfsname = DS::String::Format("%s%s_%s.list", DS::Settings::AuthRoot().c_str(),
                                            directory.c_str(), fileext.c_str());
    DS::AuthManifest mfs;
    DS::NetResultCode result = mfs.loadManifest(mfsname.c_str());
    client.m_buffer.write<uint32_t>(result);

    if (result != DS::e_NetSuccess) {
        fprintf(stderr, "[Auth] %s requested invalid manifest %s\n",
                DS::SockIpAddress(client.m_sock).c_str(), mfsname.c_str());
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
    DS::String filename = DS::CryptRecvString(client.m_sock, client.m_crypt);

    // Ensure filename is jailed to our data path
    if (filename.find("..") != -1) {
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Chunk offset
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }
    filename.replace("\\", "/");

    filename = DS::Settings::AuthRoot() + filename;
    DS::FileStream* stream = new DS::FileStream();
    try {
        stream->open(filename.c_str(), "rb");
    } catch (DS::FileIOException ex) {
        fprintf(stderr, "[Auth] Could not open file %s: %s\n[Auth] Requested by %s\n",
                filename.c_str(), ex.what(), DS::SockIpAddress(client.m_sock).c_str());
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Chunk offset
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        delete stream;
        return;
    }

    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    client.m_buffer.write<uint32_t>(stream->size());
    client.m_buffer.write<uint32_t>(stream->tell());

    uint8_t data[CHUNK_SIZE];
    if (stream->size() > CHUNK_SIZE) {
        client.m_buffer.write<uint32_t>(CHUNK_SIZE);
        stream->readBytes(data, CHUNK_SIZE);
        client.m_buffer.writeBytes(data, CHUNK_SIZE);
        client.m_downloads[transId] = stream;
    } else {
        client.m_buffer.write<uint32_t>(stream->size());
        stream->readBytes(data, stream->size());
        client.m_buffer.writeBytes(data, stream->size());
        delete stream;
    }

    SEND_REPLY();
}

void cb_downloadNext(AuthServer_Private& client)
{
    START_REPLY(e_AuthToCli_FileDownloadChunk);

    // Trans ID
    uint32_t transId = DS::CryptRecvValue<uint32_t>(client.m_sock, client.m_crypt);
    client.m_buffer.write<uint32_t>(transId);

    std::map<uint32_t, DS::Stream*>::iterator fi = client.m_downloads.find(transId);
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
        delete fi->second;
        client.m_downloads.erase(fi);
    }

    SEND_REPLY();
}

void* wk_authWorker(void* sockp)
{
    AuthServer_Private client;

    pthread_mutex_lock(&s_authClientMutex);
    client.m_sock = reinterpret_cast<DS::SocketHandle>(sockp);
    s_authClients.push_back(&client);
    pthread_mutex_unlock(&s_authClientMutex);

    try {
        auth_init(client);
        client.m_player.m_playerId = 0;

        for ( ;; ) {
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
            case e_CliToAuth_FileListRequest:
                cb_fileList(client);
                break;
            case e_CliToAuth_FileDownloadRequest:
                cb_downloadStart(client);
                break;
            case e_CliToAuth_FileDownloadChunkAck:
                cb_downloadNext(client);
                break;
            default:
                /* Invalid message */
                fprintf(stderr, "[Auth] Got invalid message ID %d from %s\n",
                        msgId, DS::SockIpAddress(client.m_sock).c_str());
                DS::CloseSock(client.m_sock);
                throw DS::SockHup();
            }
        }
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[Auth] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
    } catch (DS::SockHup) {
        // Socket closed...
    }

    pthread_mutex_lock(&s_authClientMutex);
    std::list<AuthServer_Private*>::iterator client_iter = s_authClients.begin();
    while (client_iter != s_authClients.end()) {
        if (*client_iter == &client)
            client_iter = s_authClients.erase(client_iter);
        else
            ++client_iter;
    }
    pthread_mutex_unlock(&s_authClientMutex);

    DS::CryptStateFree(client.m_crypt);
    DS::FreeSock(client.m_sock);
    return 0;
}

void DS::AuthServer_Init()
{
    s_authServerRunning = dm_auth_init();
    if (s_authServerRunning) {
        pthread_mutex_init(&s_authClientMutex, 0);
        pthread_create(&s_authDaemonThread, 0, &dm_authDaemon, 0);
    } else {
        fprintf(stderr, "Warning: Auth server not running!\n");
    }
}

void DS::AuthServer_Add(DS::SocketHandle client)
{
#ifdef DEBUG
    if (s_commdebug)
        printf("Connecting AUTH on %s\n", DS::SockIpAddress(client).c_str());
#endif

    pthread_t threadh;
    pthread_create(&threadh, 0, &wk_authWorker, reinterpret_cast<void*>(client));
    pthread_detach(threadh);
}

void DS::AuthServer_Shutdown()
{
    if (s_authServerRunning) {
        s_authChannel.putMessage(e_AuthShutdown);
        pthread_join(s_authDaemonThread, 0);
    }
}
