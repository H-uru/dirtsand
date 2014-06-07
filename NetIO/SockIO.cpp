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

#include "SockIO.h"
#include "errors.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <mutex>

static const int SOCK_NO  = 0;
static const int SOCK_YES = 1;

struct SocketHandle_Private
{
    int m_sockfd;
    union {
        sockaddr         m_addr;
        sockaddr_in      m_in4addr;
        sockaddr_in6     m_in6addr;
        sockaddr_storage m_addrMax;
    };
    socklen_t m_addrLen;
    std::mutex m_sendLock;

    SocketHandle_Private() : m_addrLen(sizeof(m_addrMax)) { }
};

static void* get_in_addr(SocketHandle_Private* sock)
{
    if (sock->m_addr.sa_family == AF_INET)
        return &sock->m_in4addr.sin_addr;
    return &sock->m_in6addr.sin6_addr;
}

static uint16_t get_in_port(SocketHandle_Private* sock)
{
    if (sock->m_addr.sa_family == AF_INET)
        return ntohs(sock->m_in4addr.sin_port);
    return ntohs(sock->m_in6addr.sin6_port);
}

DS::SocketHandle DS::BindSocket(const char* address, const char* port)
{
    int result;
    int sockfd;

    addrinfo info;
    memset(&info, 0, sizeof(info));
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = AI_PASSIVE;

    addrinfo* addrList;
    result = getaddrinfo(address, port, &info, &addrList);
    DS_PASSERT(result == 0);

    addrinfo* addr_iter;
    for (addr_iter = addrList; addr_iter != 0; addr_iter = addr_iter->ai_next) {
        sockfd = socket(addr_iter->ai_family, addr_iter->ai_socktype,
                        addr_iter->ai_protocol);
        if (sockfd == -1)
            continue;

        // Avoid annoying "Address already in use" messages when restarting
        // the server.
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &SOCK_YES, sizeof(SOCK_YES));
        if (bind(sockfd, addr_iter->ai_addr, addr_iter->ai_addrlen) == 0)
            break;
        fprintf(stderr, "[Bind] %s\n", strerror(errno));

        // Couldn't bind, try the next one
        close(sockfd);
    }
    freeaddrinfo(addrList);

    // Die if we didn't get a successful socket
    DS_PASSERT(addr_iter);

    SocketHandle_Private* sockinfo = new SocketHandle_Private();
    sockinfo->m_sockfd = sockfd;
    result = getsockname(sockfd, &sockinfo->m_addr, &sockinfo->m_addrLen);
    if (result != 0) {
        delete sockinfo;
        DS_PASSERT(0);
    }
    return reinterpret_cast<SocketHandle>(sockinfo);
}

void DS::ListenSock(const DS::SocketHandle sock, int backlog)
{
    DS_DASSERT(sock);
    int result = listen(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd, backlog);
    DS_PASSERT(result == 0);
}

DS::SocketHandle DS::AcceptSock(const DS::SocketHandle sock)
{
    DS_DASSERT(sock);
    SocketHandle_Private* sockp = reinterpret_cast<SocketHandle_Private*>(sock);

    SocketHandle_Private* client = new SocketHandle_Private();
    client->m_sockfd = accept(sockp->m_sockfd, &client->m_addr, &client->m_addrLen);
    if (client->m_sockfd < 0) {
        delete client;
        if (errno == EINVAL) {
            throw DS::SockHup();
        } else if (errno == ECONNABORTED) {
            return 0;
        } else {
            fprintf(stderr, "Socket closed: %s\n", strerror(errno));
            DS_DASSERT(0);
        }
    }
    timeval tv;
    // Client pings us every 30 seconds. A timeout of 45 gives us some wiggle room
    // so networks can suck without kicking a client off.
    tv.tv_sec = 45;
    tv.tv_usec = 0;
    setsockopt(client->m_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    // Allow 50ms on blocking sends to account for net suckiness
    tv.tv_sec = 0;
    tv.tv_usec = (50 * 1000);
    setsockopt(client->m_sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    // eap-tastic protocols require Nagle's algo be disabled
    setsockopt(client->m_sockfd, IPPROTO_TCP, TCP_NODELAY, &SOCK_YES, sizeof(SOCK_YES));
    return reinterpret_cast<SocketHandle>(client);
}

void DS::CloseSock(DS::SocketHandle sock)
{
    DS_DASSERT(sock);
    shutdown(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd, SHUT_RDWR);
    close(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd);
}

void DS::FreeSock(DS::SocketHandle sock)
{
    CloseSock(sock);
    delete reinterpret_cast<SocketHandle_Private*>(sock);
}

DS::String DS::SockIpAddress(const DS::SocketHandle sock)
{
    char addrbuf[256];
    SocketHandle_Private* sockp = reinterpret_cast<SocketHandle_Private*>(sock);
    inet_ntop(sockp->m_addr.sa_family, get_in_addr(sockp), addrbuf, 256);
    return String::Format("%s/%u", addrbuf, get_in_port(sockp));
}

uint32_t DS::GetAddress4(const char* lookup)
{
    addrinfo info;
    memset(&info, 0, sizeof(info));
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = 0;

    addrinfo* addrList;
    int result = getaddrinfo(lookup, 0, &info, &addrList);
    DS_PASSERT(result == 0);
    DS_PASSERT(addrList != 0);
    uint32_t addr = reinterpret_cast<sockaddr_in*>(addrList->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(addrList);

    return ntohl(addr);
}

void DS::SendBuffer(const DS::SocketHandle sock, const void* buffer, size_t size, SendFlag mode)
{
    int32_t flags = (mode & DS::e_SendNonBlocking) ? MSG_DONTWAIT : 0;
    bool retry = !(mode & DS::e_SendNoRetry);
    std::lock_guard<std::mutex> guard(reinterpret_cast<SocketHandle_Private*>(sock)->m_sendLock);
    do {
        ssize_t bytes = send(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd,
                             buffer, size, flags);
        if (bytes < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                throw DS::SockHup();
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (retry)
                    continue;
                else {
                    CloseSock(sock);
                    throw DS::SockHup();
                }
            }
        } else if (bytes == 0)
            throw DS::SockHup();
        DS_PASSERT(bytes > 0);

        size -= bytes;
        buffer = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(buffer) + bytes);
    } while ((size > 0) && retry);

    if (size > 0) {
        CloseSock(sock);
        throw DS::SockHup();
    }
}

void DS::SendFile(const DS::SocketHandle sock, const void* buffer, size_t bufsz,
                  int fd, off_t* offset, size_t fdsz)
{
    SocketHandle_Private* imp = reinterpret_cast<SocketHandle_Private*>(sock);
    std::lock_guard<std::mutex> guard(imp->m_sendLock);

    // Send the prepended buffer
    setsockopt(imp->m_sockfd, IPPROTO_TCP, TCP_CORK, &SOCK_YES, sizeof(SOCK_YES));
    while (bufsz > 0) {
        ssize_t bytes = send(imp->m_sockfd, buffer, bufsz, 0);
        if (bytes < 0 && (errno == EPIPE || errno == ECONNRESET))
            throw DS::SockHup();
        else if (bytes == 0)
            throw DS::SockHup();
        DS_PASSERT(bytes > 0);

        bufsz -= bytes;
        buffer = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(buffer) + bytes);
    }

    // Now send the file data via a system call
    while (fdsz > 0) {
        ssize_t bytes = sendfile(imp->m_sockfd, fd, offset, fdsz);
        if (bytes < 0 && (errno == EAGAIN))
            continue;
        else if (bytes < 0 && (errno == EPIPE || errno == ECONNRESET))
            throw DS::SockHup();
        else if (bytes == 0)
            throw DS::SockHup();
        DS_PASSERT(bytes > 0);
        fdsz -= bytes;
    }
    setsockopt(imp->m_sockfd, IPPROTO_TCP, TCP_CORK, &SOCK_NO, sizeof(SOCK_NO));
}

void DS::RecvBuffer(const DS::SocketHandle sock, void* buffer, size_t size)
{
    while (size > 0) {
        ssize_t bytes = recv(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd,
                             buffer, size, 0);
        if (bytes < 0 && (errno == ECONNRESET || errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE))
            throw DS::SockHup();
        if (bytes < 0 && errno == EINTR)
            continue;
        else if (bytes == 0)
            throw DS::SockHup();
        DS_PASSERT(bytes > 0);

        size -= bytes;
        buffer = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(buffer) + bytes);
    }
}

size_t DS::PeekSize(const SocketHandle sock)
{
    uint8_t buffer[256];
    ssize_t bytes = recv(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd,
                         buffer, 256, MSG_PEEK | MSG_TRUNC);

    if (bytes < 0 && (errno == ECONNRESET || errno == EAGAIN || errno == EWOULDBLOCK))
        throw DS::SockHup();
    else if (bytes == 0)
        throw DS::SockHup();
    DS_PASSERT(bytes > 0);

    return static_cast<size_t>(bytes);
}
