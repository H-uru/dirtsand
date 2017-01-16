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

#ifndef _DS_SOCKIO_H
#define _DS_SOCKIO_H

#include "streams.h"
#include <exception>

// Don't allow the client to send payloads > 128KB in size
#define MAX_PAYLOAD_SIZE (128 * 1024)

namespace DS
{
    typedef void* SocketHandle;

    SocketHandle BindSocket(const char* address, const char* port);
    void ListenSock(const SocketHandle sock, int backlog = 10);
    SocketHandle AcceptSock(const SocketHandle sock);
    void CloseSock(SocketHandle sock);
    void FreeSock(SocketHandle sock);

    ST::string SockIpAddress(const SocketHandle sock);
    uint32_t GetAddress4(const char* lookup);
    int SockFd(const SocketHandle sock);

    void SendBuffer(const SocketHandle sock, const void* buffer, size_t size);
    void SendFile(const SocketHandle sock, const void* buffer, size_t bufsz,
                  int fd, off_t* offset, size_t fdsz);
    void RecvBuffer(const SocketHandle sock, void* buffer, size_t size);
    size_t PeekSize(const SocketHandle sock);

    template <typename tp>
    inline tp RecvValue(const SocketHandle sock)
    {
        tp value;
        RecvBuffer(sock, &value, sizeof(value));
        return value;
    }

    class PacketSizeOutOfBounds : public std::exception
    {
    public:
        PacketSizeOutOfBounds(uint32_t requestedSize) throw()
            : m_requestedSize(requestedSize) { }
        virtual ~PacketSizeOutOfBounds() throw() { }

        virtual const char* what() const throw()
        { return "Packet size is too large"; }

        uint32_t requestedSize() const { return m_requestedSize; }

    private:
        uint32_t m_requestedSize;
    };

    inline uint32_t RecvSize(const SocketHandle sock, uint32_t maxSize = MAX_PAYLOAD_SIZE)
    {
        uint32_t size = RecvValue<uint32_t>(sock);
        if (size > maxSize)
            throw PacketSizeOutOfBounds(size);
        return size;
    }

    class SockHup : public std::exception
    {
    public:
        SockHup() throw() { }
        virtual ~SockHup() throw() { }

        virtual const char* what() const throw()
        { return "Socket closed"; }
    };
}

#endif
