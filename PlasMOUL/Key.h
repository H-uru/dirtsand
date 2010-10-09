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

#ifndef _MOUL_KEY_H
#define _MOUL_KEY_H

#include "streams.h"

namespace MOUL
{
    class Location
    {
    public:
        enum Flags
        {
            e_LocalOnly = (1<<0),
            e_Volatile  = (1<<1),
            e_Reserved  = (1<<2),
            e_BuiltIn   = (1<<3),
            e_Itinerant = (1<<4),
        };

        static Location Invalid;
        static Location Virtual;

        Location(uint32_t sequence = 0xFFFFFFFF, uint16_t flags = 0)
            : m_sequence(sequence), m_flags(flags) { }
        Location(int prefix, int page, uint16_t flags);

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

        bool operator==(const Location& other)
        { return m_sequence == other.m_sequence; }

        bool operator!=(const Location& other)
        { return m_sequence != other.m_sequence; }

        bool operator<(const Location& other)
        { return m_sequence < other.m_sequence; }

        bool operator>(const Location& other)
        { return m_sequence > other.m_sequence; }

        bool operator<=(const Location& other)
        { return m_sequence <= other.m_sequence; }

        bool operator>=(const Location& other)
        { return m_sequence >= other.m_sequence; }

    public:
        uint32_t m_sequence;
        uint16_t m_flags;
    };

    class Uoid
    {
    public:
        Uoid()
            : m_loadMask(0xFF), m_type(0x8000), m_id(0), m_cloneId(0),
              m_clonePlayerId(0) { }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

    public:
        Location m_location;
        uint8_t m_loadMask;
        uint16_t m_type;
        DS::String m_name;
        uint32_t m_id, m_cloneId, m_clonePlayerId;
    };
}

#endif
