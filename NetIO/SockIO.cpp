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

#include "SockIO.h"
#include "errors.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

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
    return reinterpret_cast<SocketHandle>(client);
}

void DS::CloseSock(DS::SocketHandle sock)
{
    DS_DASSERT(sock);
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

void DS::SendBuffer(const DS::SocketHandle sock, const void* buffer, size_t size)
{
    while (size > 0) {
        ssize_t bytes = send(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd,
                            buffer, size, 0);
        if (bytes < 0 && (errno == EPIPE || errno == ECONNRESET))
            throw DS::SockHup();
        else if (bytes == 0)
            throw DS::SockHup();
        DS_PASSERT(bytes > 0);

        size -= bytes;
        buffer = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(buffer) + bytes);
    }
}

void DS::RecvBuffer(const DS::SocketHandle sock, void* buffer, size_t size)
{
    while (size > 0) {
        ssize_t bytes = recv(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd,
                             buffer, size, 0);
        if (bytes < 0 && errno == ECONNRESET)
            throw DS::SockHup();
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

    if (bytes < 0 && errno == ECONNRESET)
        throw DS::SockHup();
    else if (bytes == 0)
        throw DS::SockHup();
    DS_PASSERT(bytes > 0);

    return static_cast<size_t>(bytes);
}
