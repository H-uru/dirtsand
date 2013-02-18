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

namespace DS
{
    enum SendFlag
    {
        e_SendDefault,
        e_SendNonBlocking = (1<<0),
        e_SendNoRetry = (1<<1),
        e_SendAllowFailure = (1<<2),

        e_SendImmediately = (e_SendNonBlocking | e_SendNoRetry),
        e_SendSpam = (e_SendImmediately | e_SendAllowFailure)
    };

    typedef void* SocketHandle;

    SocketHandle BindSocket(const char* address, const char* port);
    void ListenSock(const SocketHandle sock, int backlog = 10);
    SocketHandle AcceptSock(const SocketHandle sock);
    void CloseSock(SocketHandle sock);
    void FreeSock(SocketHandle sock);

    String SockIpAddress(const SocketHandle sock);
    uint32_t GetAddress4(const char* lookup);

    void SendBuffer(const SocketHandle sock, const void* buffer,
                    size_t size, SendFlag mode=e_SendDefault);
    void RecvBuffer(const SocketHandle sock, void* buffer, size_t size);
    size_t PeekSize(const SocketHandle sock);

    template <typename tp>
    inline tp RecvValue(const SocketHandle sock)
    {
        tp value;
        RecvBuffer(sock, &value, sizeof(value));
        return value;
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
