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

#ifndef _DS_CRYPTIO_H
#define _DS_CRYPTIO_H

#include "SockIO.h"
#include <algorithm>

#define BYTE_SWAP_BUFFER(buffer, size) \
    { \
        for (size_t i=0; i<(size/2); ++i) \
            std::swap(buffer[i], buffer[size-1-i]); \
    }

namespace DS
{
    /* IMPORTANT NOTE:  OpenSSL and DirtSand store keys in Big-Endian byte
     * order.  Plasma, on the other hand, expects keys in Little-Endian byte
     * order, so anything you give to the client MUST be byte-swapped to
     * play nicely with Plasma.
     */

    void CryptCalcX(uint8_t* X, const uint8_t* N, const uint8_t* K, uint32_t base);
    void PrintClientKeys(const uint8_t* X, const uint8_t* N);
}

#endif
