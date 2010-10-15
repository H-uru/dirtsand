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

void DS::ListenSock(DS::SocketHandle sock, int backlog)
{
    DS_DASSERT(sock);
    int result = listen(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd, backlog);
    DS_PASSERT(result == 0);
}

DS::SocketHandle DS::AcceptSock(DS::SocketHandle sock)
{
    DS_DASSERT(sock);
    SocketHandle_Private* sockp = reinterpret_cast<SocketHandle_Private*>(sock);

    SocketHandle_Private* client = new SocketHandle_Private();
    client->m_sockfd = accept(sockp->m_sockfd, &client->m_addr, &client->m_addrLen);
    if (client->m_sockfd < 0) {
        delete client;
        if (errno != EINVAL) {
            fprintf(stderr, "Socket closed: %s\n", strerror(errno));
            DS_DASSERT(0);
        }
        return 0;
    }
    return reinterpret_cast<SocketHandle>(client);
}

void DS::CloseSock(DS::SocketHandle sock)
{
    DS_DASSERT(sock);
    shutdown(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd, SHUT_RDWR);
}

void DS::FreeSock(DS::SocketHandle sock)
{
    CloseSock(sock);
    delete reinterpret_cast<SocketHandle_Private*>(sock);
}

DS::String DS::SockIpAddress(DS::SocketHandle sock)
{
    char addrbuf[256];
    SocketHandle_Private* sockp = reinterpret_cast<SocketHandle_Private*>(sock);
    inet_ntop(sockp->m_addr.sa_family, get_in_addr(sockp), addrbuf, 256);
    return DS::String(addrbuf);
}

void DS::SendBuffer(DS::SocketHandle sock, const void* buffer, size_t size)
{
    ssize_t bytes = send(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd,
                         buffer, size, 0);
    DS_PASSERT(static_cast<size_t>(bytes) == size);
}

void DS::RecvBuffer(DS::SocketHandle sock, void* buffer, size_t size)
{
    ssize_t bytes = recv(reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd,
                         buffer, size, 0);
    if (bytes < 0 && errno == ECONNRESET)
        throw DS::SockHup();
    else if (bytes == 0)
        throw DS::SockHup();
    DS_PASSERT(static_cast<size_t>(bytes) == size);
}

/*
DS::Blob* DS::RecvBlob(DS::SocketHandle sock)
{
    uint8_t peekbuf[32];
    int sockfd = reinterpret_cast<SocketHandle_Private*>(sock)->m_sockfd;
    ssize_t bytes = recv(sockfd, peekbuf, 32, MSG_PEEK);

    if (!bytes) {
        return 0;
    } else {
        uint8_t* buffer = new uint8_t[bytes];
        recv(sockfd, buffer, bytes, 0);
        return new Blob(buffer, bytes);
    }
}
*/
