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

#ifndef _DS_UUID_H
#define _DS_UUID_H

#include "streams.h"

namespace DS
{
    class Uuid
    {
    public:
        Uuid() { memset(m_bytes, 0, sizeof(m_bytes)); }

        Uuid(uint32_t data1, uint16_t data2, uint16_t data3, const uint8_t* data4)
            : m_data1(data1), m_data2(data2), m_data3(data3)
        { memcpy(m_data4, data4, sizeof(m_data4)); }

        Uuid(const uint8_t* bytes)
        { memcpy(m_bytes, bytes, sizeof(m_bytes)); }

        Uuid(const char* struuid);

        bool operator==(const Uuid& other) const
        { return memcmp(m_bytes, other.m_bytes, sizeof(m_bytes)) == 0; }

        bool operator!=(const Uuid& other) const
        { return memcmp(m_bytes, other.m_bytes, sizeof(m_bytes)) != 0; }

        bool isNull() const { return operator==(Uuid()); }

        void read(Stream* stream);
        void write(Stream* stream) const;

        ST::string toString() const;

    public:
        union {
            struct {
                uint32_t m_data1;
                uint16_t m_data2, m_data3;
                uint8_t m_data4[8];
            };
            uint8_t m_bytes[16];
        };
    };

    struct UuidHash
    {
        // Simple but effective
        size_t operator()(const Uuid& value) const { return value.m_data1; }
    };
}

#endif
