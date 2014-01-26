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
        static Location GlobalServer;

        Location(uint32_t sequence = 0xFFFFFFFF, uint16_t flags = 0)
            : m_sequence(sequence), m_flags(flags) { }
        Location(int prefix, int page, uint16_t flags);

        void read(DS::Stream* stream);
        void write(DS::Stream* stream) const;

        bool operator==(const Location& other) const
        { return m_sequence == other.m_sequence; }

        bool operator!=(const Location& other) const
        { return m_sequence != other.m_sequence; }

        bool operator<(const Location& other) const
        { return m_sequence < other.m_sequence; }

        bool operator>(const Location& other) const
        { return m_sequence > other.m_sequence; }

        bool operator<=(const Location& other) const
        { return m_sequence <= other.m_sequence; }

        bool operator>=(const Location& other) const
        { return m_sequence >= other.m_sequence; }

        static Location Make(int32_t prefix, uint16_t page, uint16_t flags)
        {
            if (prefix < 0)
                return Location(page - (prefix << 16) + 0xFF000001, flags);
            else
                return Location(page + (prefix << 16) + 0x00000021, flags);
        }

    public:
        uint32_t m_sequence;
        uint16_t m_flags;
    };

    class Uoid
    {
    public:
        Uoid()
            : m_loadMask(0xFF), m_type(0x8000), m_id(), m_cloneId(),
              m_clonePlayerId() { }

        Uoid(const Location& loc, uint16_t type, const DS::String& name,
             uint32_t id = 0, uint8_t loadMask = 0xFF)
            : m_location(loc), m_loadMask(loadMask), m_type(type),
              m_name(name), m_id(id), m_cloneId(), m_clonePlayerId() { }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream) const;

        bool isNull() const { return m_type == 0x8000; }

        bool operator==(const Uoid& other) const
        {
            return (m_location == other.m_location) && (m_type == other.m_type)
                   && (m_name == other.m_name) && (m_id == other.m_id)
                   && (m_cloneId == other.m_cloneId)
                   && (m_clonePlayerId == other.m_clonePlayerId);
        }
        bool operator!=(const Uoid& other) const { return !operator==(other); }

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
        /* Fixed keys */
        static Key AvatarMgrKey;
        static Key NetClientMgrKey;

        Key() : m_data() { }

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
            if (m_data && (--m_data->m_refs == 0))
                delete m_data;
        }

        Key& operator=(const Key& copy)
        {
            if (copy.m_data)
                ++copy.m_data->m_refs;
            if (m_data && (--m_data->m_refs == 0))
                delete m_data;
            m_data = copy.m_data;
            return *this;
        }

        Key& operator=(const Uoid& copy)
        {
            if (m_data && (--m_data->m_refs == 0))
                delete m_data;
            m_data = new _keydata;
            m_data->m_refs = 1;
            m_data->m_uoid = copy;
            return *this;
        }

        bool operator==(const Key& other) const { return m_data == other.m_data; }
        bool operator!=(const Key& other) const { return m_data != other.m_data; }
        bool isNull() const { return m_data == nullptr; }

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
        void write(DS::Stream* stream) const;

    private:
        struct _keydata
        {
            Uoid m_uoid;
            std::atomic_int m_refs;
        }* m_data;
    };

    struct UoidHash : public DS::StringHash
    {
        size_t operator()(const Uoid& value) const
        {
            return DS::StringHash::operator()(value.m_name)
                   ^ (value.m_location.m_sequence + (value.m_type << 8));
        }
    };
}

#endif
