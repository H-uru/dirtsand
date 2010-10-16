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

#include "FileServer.h"
#include "FileManifest.h"
#include "settings.h"
#include "errors.h"
#include <pthread.h>
#include <unistd.h>
#include <list>

struct FileServer_Private
{
    DS::SocketHandle m_sock;
    DS::BufferStream m_buffer;
    uint32_t m_readerId;
};

static std::list<FileServer_Private*> s_clients;
static pthread_mutex_t s_clientMutex;

enum FileServer_MsgIds
{
    e_CliToFile_PingRequest = 0, e_CliToFile_BuildIdRequest = 10,
    e_CliToFile_ManifestRequest = 20, e_CliToFile_DownloadRequest = 21,
    e_CliToFile_ManifestEntryAck = 22, e_CliToFile_DownloadChunkAck = 23,

    e_FileToCli_PingReply = 0, e_FileToCli_BuildIdReply = 10,
    e_FileToCli_BuildIdUpdate = 11, e_FileToCli_ManifestReply = 20,
    e_FileToCli_FileDownloadReply = 21,
};

#define START_REPLY(msgId) \
    client.m_buffer.truncate(); \
    client.m_buffer.write<uint32_t>(0); \
    client.m_buffer.write<uint32_t>(msgId)

#define SEND_REPLY() \
    client.m_buffer.seek(0, SEEK_SET); \
    client.m_buffer.write<uint32_t>(client.m_buffer.size()); \
    DS::SendBuffer(client.m_sock, client.m_buffer.buffer(), client.m_buffer.size())

void file_init(FileServer_Private& client)
{
    /* File server header:  size, buildId, serverType */
    uint32_t size = DS::RecvValue<uint32_t>(client.m_sock);
    DS_PASSERT(size == 12);
    DS::RecvValue<uint32_t>(client.m_sock);
    DS::RecvValue<uint32_t>(client.m_sock);
}

void cb_ping(FileServer_Private& client)
{
    START_REPLY(e_FileToCli_PingReply);

    // Ping time
    client.m_buffer.write<uint32_t>(DS::RecvValue<uint32_t>(client.m_sock));

    SEND_REPLY();
}

void cb_buildId(FileServer_Private& client)
{
    START_REPLY(e_FileToCli_PingReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::RecvValue<uint32_t>(client.m_sock));

    // Result
    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);

    // Build ID
    client.m_buffer.write<uint32_t>(CLIENT_BUILD_ID);

    SEND_REPLY();
}

void cb_manifest(FileServer_Private& client)
{
    START_REPLY(e_FileToCli_ManifestReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::RecvValue<uint32_t>(client.m_sock));

    // Manifest name
    chr16_t mfsbuf[260];
    DS::RecvBuffer(client.m_sock, mfsbuf, 260 * sizeof(chr16_t));
    mfsbuf[259] = 0;
    DS::String mfsname = DS::String::FromUtf16(mfsbuf);

    // Build ID
    uint32_t buildId = DS::RecvValue<uint32_t>(client.m_sock);
    if (buildId && buildId != CLIENT_BUILD_ID) {
        fprintf(stderr, "[File] Wrong Build ID from %s: %d\n",
                DS::SockIpAddress(client.m_sock).c_str(), buildId);
        DS::CloseSock(client.m_sock);
        return;
    }

    // Manifest may not have any path characters
    if (mfsname.find(".") != -1 || mfsname.find("/") != -1
        || mfsname.find("\\") != -1 || mfsname.find(":") != -1) {
        fprintf(stderr, "[File] Invalid manifest request from %s: %s\n",
                DS::SockIpAddress(client.m_sock).c_str(), mfsname.c_str());
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Reader ID
        client.m_buffer.write<uint32_t>(0);     // File count
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }

    DS::FileManifest manifest;
    mfsname = DS::Settings::FileRoot() + mfsname + ".mfs";
    DS::NetResultCode result = manifest.loadManifest(mfsname.c_str());
    client.m_buffer.write<uint32_t>(result);

    if (result != DS::e_NetSuccess) {
        fprintf(stderr, "[File] %s requested invalid manifest %s\n",
                DS::SockIpAddress(client.m_sock).c_str(), mfsname.c_str());
        client.m_buffer.write<uint32_t>(0);     // Reader ID
        client.m_buffer.write<uint32_t>(0);     // File count
        client.m_buffer.write<uint32_t>(0);     // Data packet size
    } else {
        client.m_buffer.write<uint32_t>(++client.m_readerId);
        client.m_buffer.write<uint32_t>(manifest.fileCount());
        uint32_t sizeLocation = client.m_buffer.tell();
        client.m_buffer.write<uint32_t>(0);
        uint32_t dataSize = manifest.encodeToStream(&client.m_buffer);
        client.m_buffer.seek(sizeLocation, SEEK_SET);
        client.m_buffer.write<uint32_t>(dataSize);
    }

    SEND_REPLY();
}

void cb_manifestAck(FileServer_Private& client)
{
    /* This is TCP, nobody cares about this ack... */
    DS::RecvValue<uint32_t>(client.m_sock);     // Trans ID
    DS::RecvValue<uint32_t>(client.m_sock);     // Reader ID
}

void* wk_fileServ(void* sockp)
{
    FileServer_Private client;

    pthread_mutex_lock(&s_clientMutex);
    client.m_sock = reinterpret_cast<DS::SocketHandle>(sockp);
    client.m_readerId = 0;
    s_clients.push_back(&client);
    pthread_mutex_unlock(&s_clientMutex);

    try {
        file_init(client);

        for ( ;; ) {
            DS::RecvValue<uint32_t>(client.m_sock);  // Message size
            uint32_t msgId = DS::RecvValue<uint32_t>(client.m_sock);
            switch (msgId) {
            case e_CliToFile_PingRequest:
                cb_ping(client);
                break;
            case e_CliToFile_BuildIdRequest:
                cb_buildId(client);
                break;
            case e_CliToFile_ManifestRequest:
                cb_manifest(client);
                break;
            case e_CliToFile_ManifestEntryAck:
                cb_manifestAck(client);
                break;
            //case e_CliToFile_DownloadRequest:
            //    cb_downloadStart(client);
            //    break;
            //case e_CliToFile_DownloadChunkAck:
            //    cb_downloadNext(client);
            //    break;
            default:
                /* Invalid message */
                fprintf(stderr, "[File] Got invalid message ID %d from %s\n",
                        msgId, DS::SockIpAddress(client.m_sock).c_str());
                DS::CloseSock(client.m_sock);
                throw DS::SockHup();
            }
        }
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[File] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
    } catch (DS::SockHup) {
        // Socket closed...
    }

    pthread_mutex_lock(&s_clientMutex);
    std::list<FileServer_Private*>::iterator client_iter = s_clients.begin();
    while (client_iter != s_clients.end()) {
        if (*client_iter == &client)
            client_iter = s_clients.erase(client_iter);
        else
            ++client_iter;
    }
    pthread_mutex_unlock(&s_clientMutex);

    DS::FreeSock(client.m_sock);
    return 0;
}

void DS::FileServer_Init()
{
    pthread_mutex_init(&s_clientMutex, 0);
}

void DS::FileServer_Add(DS::SocketHandle client)
{
    pthread_t threadh;
    pthread_create(&threadh, 0, &wk_fileServ, reinterpret_cast<void*>(client));
    pthread_detach(threadh);
}

void DS::FileServer_Shutdown()
{
    pthread_mutex_lock(&s_clientMutex);
    std::list<FileServer_Private*>::iterator client_iter;
    for (client_iter = s_clients.begin(); client_iter != s_clients.end(); ++client_iter)
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
        fprintf(stderr, "[File] Clients didn't die after 5 seconds!");
    pthread_mutex_destroy(&s_clientMutex);
}
