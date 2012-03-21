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

#ifndef _DS_CRYPTIO_H
#define _DS_CRYPTIO_H

#include "Types/ShaHash.h"
#include "SockIO.h"
#include <algorithm>

#define BYTE_SWAP_BUFFER(buffer, size) \
    { \
        for (size_t i=0; i<((size)/2); ++i) \
            std::swap(buffer[i], buffer[(size)-1-i]); \
    }

namespace DS
{
    /* IMPORTANT NOTE:  OpenSSL and DirtSand store keys in Big-Endian byte
     * order.  Plasma, on the other hand, expects keys in Little-Endian byte
     * order, so anything you give to the client MUST be byte-swapped to
     * play nicely with Plasma.
     */

    enum ConnectMsg
    {
        e_CliToServConnect, e_ServToCliEncrypt, e_ServToCliError
    };

    void GenPrimeKeys(uint8_t* N, uint8_t* K);
    void CryptCalcX(uint8_t* X, const uint8_t* N, const uint8_t* K, uint32_t base);

    void CryptEstablish(uint8_t* seed, uint8_t* key, const uint8_t* N,
                        const uint8_t* K, uint8_t* Y);

    typedef void* CryptState;

    CryptState CryptStateInit(const uint8_t* key, size_t size);
    void CryptStateFree(CryptState state);

    void CryptSendBuffer(const SocketHandle sock, CryptState crypt,
                         const void* buffer, size_t size);
    void CryptRecvBuffer(const SocketHandle sock, CryptState crypt,
                         void* buffer, size_t size);

    template <typename tp>
    inline tp CryptRecvValue(const SocketHandle sock, CryptState crypt)
    {
        tp value;
        CryptRecvBuffer(sock, crypt, &value, sizeof(value));
        return value;
    }

    inline DS::String CryptRecvString(const SocketHandle sock, CryptState crypt)
    {
        uint16_t length = CryptRecvValue<uint16_t>(sock, crypt);
        chr16_t* buffer = new chr16_t[length];
        try {
            CryptRecvBuffer(sock, crypt, buffer, length * sizeof(chr16_t));
        } catch (...) {
            delete[] buffer;
            throw;
        }
        String result = String::FromUtf16(buffer, length);
        delete[] buffer;
        return result;
    }

    ShaHash BuggyHashPassword(const String& username, const String& password);
    ShaHash BuggyHashLogin(const ShaHash& passwordHash, uint32_t serverChallenge,
                           uint32_t clientChallenge);
}

#endif
