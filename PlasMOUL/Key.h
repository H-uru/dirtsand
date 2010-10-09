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

    class Key
    {
    public:
        Key() : m_data(0) { }

        Key(const Key& copy) : m_data(copy.m_data)
        {
            if (m_data)
                ++m_data->m_refs;
        }

        Key(const Uoid& copy)
        {
            m_data = new _keydata;
            m_data->m_refs = 1;
            m_data->m_uoid = copy;
        }

        ~Key()
        {
            if (m_data && --m_data->m_refs)
                delete m_data;
        }

        Key& operator=(const Key& copy)
        {
            if (copy.m_data)
                ++copy.m_data->m_refs;
            if (m_data && --m_data->m_refs)
                delete m_data;
            m_data = copy.m_data;
            return *this;
        }

        Key& operator=(const Uoid& copy)
        {
            if (m_data && --m_data->m_refs)
                delete m_data;
            m_data = new _keydata;
            m_data->m_refs = 1;
            m_data->m_uoid = copy;
            return *this;
        }

        bool operator==(const Key& other) const { return m_data == other.m_data; }
        bool operator!=(const Key& other) const { return m_data != other.m_data; }
        bool isNull() const { return m_data == 0; }

        Location location() const { return m_data ? m_data->m_uoid.m_location : Location::Invalid; }
        uint8_t loadMask() const { return m_data ? m_data->m_uoid.m_loadMask : 0xFF; }
        uint16_t type() const { return m_data ? m_data->m_uoid.m_type : 0x8000; }
        DS::String name() const { return m_data ? m_data->m_uoid.m_name : DS::String(); }
        uint32_t id() const { return m_data ? m_data->m_uoid.m_id : 0; }
        uint32_t cloneId() const { return m_data ? m_data->m_uoid.m_cloneId : 0; }
        uint32_t clonePlayerId() const { return m_data ? m_data->m_uoid.m_clonePlayerId : 0; }

        /* Note: these will NOT keep synchronized copies.  You need some
           form of Resource Manager to handle that. */
        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

    private:
        struct _keydata
        {
            Uoid m_uoid;
            int m_refs;
        }* m_data;
    };
}

#endif
