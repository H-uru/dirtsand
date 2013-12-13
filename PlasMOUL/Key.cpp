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

#include "Key.h"

MOUL::Location MOUL::Location::Invalid      (0xFFFFFFFF, 0);
MOUL::Location MOUL::Location::Virtual      (0,          0);
MOUL::Location MOUL::Location::GlobalServer (0xFF000000, e_Reserved);

MOUL::Key MOUL::Key::AvatarMgrKey   (Uoid(Location::Virtual, 0x00F4, "kAvatarMgr_KEY"));
MOUL::Key MOUL::Key::NetClientMgrKey(Uoid(Location::Virtual, 0x0052, "kNetClientMgr_KEY"));

MOUL::Location::Location(int prefix, int page, uint16_t flags)
    : m_flags(flags)
{
    if (prefix < 0)
        m_sequence = (page & 0xFFFF) - (prefix << 16) + 0xFF000001;
    else
        m_sequence = (page & 0xFFFF) + (prefix << 16) + 33;
}

void MOUL::Location::read(DS::Stream* stream)
{
    m_sequence = stream->read<uint32_t>();
    m_flags = stream->read<uint16_t>();
}

void MOUL::Location::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_sequence);
    stream->write<uint16_t>(m_flags);
}


enum UoidContents
{
    e_HasCloneIds = (1<<0),
    e_HasLoadMask = (1<<1),
};

void MOUL::Uoid::read(DS::Stream* stream)
{
    uint8_t contents = stream->read<uint8_t>();
    m_location.read(stream);
    if (contents & e_HasLoadMask)
        m_loadMask = stream->read<uint8_t>();
    else
        m_loadMask = 0xFF;
    m_type = stream->read<uint16_t>();
    m_id = stream->read<uint32_t>();
    m_name = stream->readSafeString();
    if (contents & e_HasCloneIds) {
        m_cloneId = stream->read<uint32_t>();
        m_clonePlayerId = stream->read<uint32_t>();
    } else {
        m_cloneId = m_clonePlayerId = 0;
    }
}

void MOUL::Uoid::write(DS::Stream* stream) const
{
    uint8_t contents = 0;
    if (m_loadMask != 0xFF)
        contents |= e_HasLoadMask;
    if (m_cloneId || m_clonePlayerId)
        contents |= e_HasCloneIds;
    stream->write<uint8_t>(contents);

    m_location.write(stream);
    if (contents & e_HasLoadMask)
        stream->write<uint8_t>(m_loadMask);
    stream->write<uint16_t>(m_type);
    stream->write<uint32_t>(m_id);
    stream->writeSafeString(m_name);
    if (contents & e_HasCloneIds) {
        stream->write<uint32_t>(m_cloneId);
        stream->write<uint32_t>(m_clonePlayerId);
    }
}

void MOUL::Key::read(DS::Stream* stream)
{
    if (stream->read<bool>()) {
        Uoid data;
        data.read(stream);
        operator=(data);
    } else {
        operator=(Key());
    }
}

void MOUL::Key::write(DS::Stream* stream) const
{
    stream->write<bool>(!isNull());
    if (!isNull())
        m_data->m_uoid.write(stream);
}
