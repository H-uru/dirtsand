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

#ifndef _DS_SOCKIO_H
#define _DS_SOCKIO_H

#include "strings.h"

namespace DS
{
    typedef void* SocketHandle;

    class Blob
    {
    public:
        Blob(const uint8_t* buffer, size_t size)
            : m_buffer(buffer), m_size(size), m_refs(1) { }

        const uint8_t* buffer() const { return m_buffer; }
        size_t size() const { return m_size; }

        void ref() { ++m_refs; }
        void unref()
        {
            if (--m_refs == 0)
                delete this;
        }

    private:
        const uint8_t* m_buffer;
        size_t m_size;
        int m_refs;

        ~Blob() { delete[] m_buffer; }
    };

    SocketHandle BindSocket(const char* address, const char* port);
    void ListenSock(SocketHandle sock, int backlog = 10);
    SocketHandle AcceptSock(SocketHandle sock);
    void CloseSock(SocketHandle sock);
    void FreeSock(SocketHandle sock);

    void SendBuffer(SocketHandle sock, const uint8_t* buffer, size_t size);
    Blob* RecvBuffer(SocketHandle sock);

    inline void SendBuffer(SocketHandle sock, const Blob* blob)
    { SendBuffer(sock, blob->buffer(), blob->size()); }
}

#endif
