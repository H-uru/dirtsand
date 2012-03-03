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
#include <list>
#include <map>
#include <thread>
#include <mutex>
#include <chrono>

struct FileServer_Private
{
    DS::SocketHandle m_sock;
    DS::BufferStream m_buffer;
    uint32_t m_readerId;

    std::map<uint32_t, DS::Stream*> m_downloads;

    ~FileServer_Private()
    {
        while (!m_downloads.empty()) {
            auto item = m_downloads.begin();
            delete item->second;
            m_downloads.erase(item);
        }
    }
};

static std::list<FileServer_Private*> s_clients;
static std::mutex s_clientMutex;

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
    START_REPLY(e_FileToCli_BuildIdReply);

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

void cb_downloadStart(FileServer_Private& client)
{
    START_REPLY(e_FileToCli_FileDownloadReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::RecvValue<uint32_t>(client.m_sock));

    // Download filename
    chr16_t buffer[260];
    DS::RecvBuffer(client.m_sock, buffer, 260 * sizeof(chr16_t));
    buffer[259] = 0;
    DS::String filename = DS::String::FromUtf16(buffer);

    // Build ID
    uint32_t buildId = DS::RecvValue<uint32_t>(client.m_sock);
    if (buildId && buildId != CLIENT_BUILD_ID) {
        fprintf(stderr, "[File] Wrong Build ID from %s: %d\n",
                DS::SockIpAddress(client.m_sock).c_str(), buildId);
        DS::CloseSock(client.m_sock);
        return;
    }

    // Ensure filename is jailed to our data path
    if (filename.find("..") != -1) {
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Reader ID
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }
    filename.replace("\\", "/");

    filename = DS::Settings::FileRoot() + filename;
    DS::FileStream* stream = new DS::FileStream();
    try {
        stream->open(filename.c_str(), "rb");
    } catch (DS::FileIOException ex) {
        fprintf(stderr, "[File] Could not open file %s: %s\n[File] Requested by %s\n",
                filename.c_str(), ex.what(), DS::SockIpAddress(client.m_sock).c_str());
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Reader ID
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        delete stream;
        return;
    }

    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    client.m_buffer.write<uint32_t>(++client.m_readerId);
    client.m_buffer.write<uint32_t>(stream->size());

    uint8_t data[CHUNK_SIZE];
    if (stream->size() > CHUNK_SIZE) {
        client.m_buffer.write<uint32_t>(CHUNK_SIZE);
        stream->readBytes(data, CHUNK_SIZE);
        client.m_buffer.writeBytes(data, CHUNK_SIZE);
        client.m_downloads[client.m_readerId] = stream;
    } else {
        client.m_buffer.write<uint32_t>(stream->size());
        stream->readBytes(data, stream->size());
        client.m_buffer.writeBytes(data, stream->size());
        delete stream;
    }

    SEND_REPLY();
}

void cb_downloadNext(FileServer_Private& client)
{
    START_REPLY(e_FileToCli_FileDownloadReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::RecvValue<uint32_t>(client.m_sock));

    uint32_t readerId = DS::RecvValue<uint32_t>(client.m_sock);
    auto fi = client.m_downloads.find(readerId);
    if (fi == client.m_downloads.end()) {
        // The last chunk was already sent, we don't care anymore
        return;
    }

    client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
    client.m_buffer.write<uint32_t>(fi->first);
    client.m_buffer.write<uint32_t>(fi->second->size());

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

void wk_fileServ(DS::SocketHandle sockp)
{
    FileServer_Private client;

    s_clientMutex.lock();
    client.m_sock = sockp;
    client.m_readerId = 0;
    s_clients.push_back(&client);
    s_clientMutex.unlock();

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
            case e_CliToFile_DownloadRequest:
                cb_downloadStart(client);
                break;
            case e_CliToFile_DownloadChunkAck:
                cb_downloadNext(client);
                break;
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

    s_clientMutex.lock();
    auto client_iter = s_clients.begin();
    while (client_iter != s_clients.end()) {
        if (*client_iter == &client)
            client_iter = s_clients.erase(client_iter);
        else
            ++client_iter;
    }
    s_clientMutex.unlock();

    DS::FreeSock(client.m_sock);
}

void DS::FileServer_Init()
{ }

void DS::FileServer_Add(DS::SocketHandle client)
{
    std::thread threadh(&wk_fileServ, client);
    threadh.detach();
}

void DS::FileServer_Shutdown()
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
        fprintf(stderr, "[File] Clients didn't die after 5 seconds!\n");
}

void DS::FileServer_DisplayClients()
{
    std::lock_guard<std::mutex> clientGuard(s_clientMutex);
    if (s_clients.size())
        printf("File Server:\n");
    for (auto client_iter = s_clients.begin(); client_iter != s_clients.end(); ++client_iter)
        printf("  * %s\n", DS::SockIpAddress((*client_iter)->m_sock).c_str());
}
