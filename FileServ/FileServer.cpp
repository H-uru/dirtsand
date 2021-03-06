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

#include "FileServer.h"
#include "FileManifest.h"
#include "settings.h"
#include "errors.h"
#include <list>
#include <map>
#include <thread>
#include <mutex>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

struct FileServer_Private
{
    DS::SocketHandle m_sock;
    DS::BufferStream m_buffer;
    uint32_t m_readerId;
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

#define SEND_FD_REPLY(fd, off, sz) \
    client.m_buffer.seek(0, SEEK_SET); \
    client.m_buffer.write<uint32_t>(client.m_buffer.size() + sz); \
    DS::SendFile(client.m_sock, client.m_buffer.buffer(), client.m_buffer.size(), (fd), &(off), (sz))

void file_init(FileServer_Private& client)
{
    /* File server header:  size, buildId, serverType */
    uint32_t size = DS::RecvValue<uint32_t>(client.m_sock);
    if (size != 12)
        throw DS::InvalidConnectionHeader();
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
    client.m_buffer.write<uint32_t>(DS::Settings::BuildId());

    SEND_REPLY();
}

void cb_manifest(FileServer_Private& client)
{
    START_REPLY(e_FileToCli_ManifestReply);

    // Trans ID
    client.m_buffer.write<uint32_t>(DS::RecvValue<uint32_t>(client.m_sock));

    // Manifest name
    char16_t mfsbuf[260];
    DS::RecvBuffer(client.m_sock, mfsbuf, sizeof(mfsbuf));
    mfsbuf[259] = 0;
    ST::string mfsname = ST::string::from_utf16(mfsbuf, ST_AUTO_SIZE, ST::substitute_invalid);

    // Build ID
    uint32_t buildId = DS::RecvValue<uint32_t>(client.m_sock);
    if (buildId && buildId != DS::Settings::BuildId()) {
        ST::printf(stderr, "[File] Wrong Build ID from {}: {}\n",
                   DS::SockIpAddress(client.m_sock), buildId);
        DS::CloseSock(client.m_sock);
        return;
    }

    // Manifest may not have any path characters
    if (mfsname.find(".") != -1 || mfsname.find("/") != -1
            || mfsname.find("\\") != -1 || mfsname.find(":") != -1) {
        ST::printf(stderr, "[File] Invalid manifest request from {}: {}\n",
                   DS::SockIpAddress(client.m_sock), mfsname);
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
        ST::printf(stderr, "[File] {} requested invalid manifest {}\n",
                   DS::SockIpAddress(client.m_sock), mfsname);
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
    // Trans ID
    uint32_t transId = DS::RecvValue<uint32_t>(client.m_sock);

    // Download filename
    char16_t buffer[260];
    DS::RecvBuffer(client.m_sock, buffer, sizeof(buffer));
    buffer[259] = 0;
    ST::string filename = ST::string::from_utf16(buffer, ST_AUTO_SIZE, ST::substitute_invalid);

    // Build ID
    uint32_t buildId = DS::RecvValue<uint32_t>(client.m_sock);
    if (buildId && buildId != DS::Settings::BuildId()) {
        ST::printf(stderr, "[File] Wrong Build ID from {}: {}\n",
                   DS::SockIpAddress(client.m_sock), buildId);
        DS::CloseSock(client.m_sock);
        return;
    }

    // Ensure filename is jailed to our data path
    if (filename.find("..") != -1) {
        START_REPLY(e_FileToCli_FileDownloadReply);
        client.m_buffer.write<uint32_t>(transId);
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Reader ID
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }
    filename = filename.replace("\\", "/");

    filename = DS::Settings::FileRoot() + filename;
    int fd = open(filename.c_str(), O_RDONLY);

    if (fd < 0) {
        ST::printf(stderr, "[File] Could not open file {}\n[File] Requested by {}\n",
                   filename, DS::SockIpAddress(client.m_sock));
        START_REPLY(e_FileToCli_FileDownloadReply);
        client.m_buffer.write<uint32_t>(transId);
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Reader ID
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();
        return;
    }

    struct stat stat_buf;
    off_t filepos = 0;
    if (fstat(fd, &stat_buf) < 0) {
        ST::printf(stderr, "[File] Could not stat file {}\n[File] Requested by {}\n",
                   filename, DS::SockIpAddress(client.m_sock));
        START_REPLY(e_FileToCli_FileDownloadReply);
        client.m_buffer.write<uint32_t>(transId);
        client.m_buffer.write<uint32_t>(DS::e_NetFileNotFound);
        client.m_buffer.write<uint32_t>(0);     // Reader ID
        client.m_buffer.write<uint32_t>(0);     // File size
        client.m_buffer.write<uint32_t>(0);     // Data packet size
        SEND_REPLY();

        close(fd);
        return;
    }

    // Attempting to use the MOUL protocol's "acking" of file chunks has a severe performance
    // penalty. For me, I see an improvement from 950 KiB/s to 14 MiB/s by simply sending the
    // whole file synchronously. We still have to chunk it, though, so the client's progress
    // bar updates correctly. Besides, this is TCP, who needs acks at this level? The drawback
    // to this is that the connection becomes unresponsive to all other traffic when a download
    // is in progress.
    while (filepos < stat_buf.st_size) {
        off_t remsz = stat_buf.st_size - filepos;
        uint32_t chunksz = remsz > CHUNK_SIZE ? CHUNK_SIZE : (uint32_t)remsz;

        START_REPLY(e_FileToCli_FileDownloadReply);
        client.m_buffer.write<uint32_t>(transId);
        client.m_buffer.write<uint32_t>(DS::e_NetSuccess);
        client.m_buffer.write<uint32_t>(client.m_readerId);     // Reader ID
        client.m_buffer.write<uint32_t>(stat_buf.st_size);      // File size
        client.m_buffer.write<uint32_t>(chunksz);               // Data packet size
        SEND_FD_REPLY(fd, filepos, chunksz);
    }

    ++client.m_readerId;
    close(fd);
}

void cb_downloadNext(FileServer_Private& client)
{
    /* This is TCP, nobody cares about this ack... */
    DS::RecvValue<uint32_t>(client.m_sock);     // TransID
    DS::RecvValue<uint32_t>(client.m_sock);     // Reader ID
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
                ST::printf(stderr, "[File] Got invalid message ID {} from {}\n",
                           msgId, DS::SockIpAddress(client.m_sock));
                DS::CloseSock(client.m_sock);
                throw DS::SockHup();
            }
        }
    } catch (const DS::SockHup&) {
        // Socket closed...
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[File] Error processing client message from {}: {}\n",
                   DS::SockIpAddress(sockp), ex.what());
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
        fputs("[File] Clients didn't die after 5 seconds!\n", stderr);
}

void DS::FileServer_DisplayClients()
{
    std::lock_guard<std::mutex> clientGuard(s_clientMutex);
    if (s_clients.size())
        fputs("File Server:\n", stdout);
    for (auto client_iter = s_clients.begin(); client_iter != s_clients.end(); ++client_iter)
        ST::printf("  * {}\n", DS::SockIpAddress((*client_iter)->m_sock));
}
