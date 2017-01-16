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

#ifndef _DS_SHAHASH_H
#define _DS_SHAHASH_H

#include "streams.h"

namespace DS
{
    class ShaHash
    {
    public:
        ShaHash() { memset(m_data, 0, sizeof(m_data)); }

        ShaHash(const uint8_t* bytes)
        { memcpy(m_data, bytes, sizeof(m_data)); }

        ShaHash(const char* struuid);

        bool operator==(const ShaHash& other) const
        { return memcmp(m_data, other.m_data, sizeof(m_data)) == 0; }

        bool operator!=(const ShaHash& other) const
        { return memcmp(m_data, other.m_data, sizeof(m_data)) != 0; }

        void read(Stream* stream);
        void write(Stream* stream) const;
        void swapBytes();

        ST::string toString() const;

        static ShaHash Sha0(const void* data, size_t size);
        static ShaHash Sha1(const void* data, size_t size);

    public:
        uint32_t m_data[5];
    };
}

#endif
