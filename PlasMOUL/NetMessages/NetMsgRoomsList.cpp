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

#include "NetMsgRoomsList.h"

void MOUL::NetMsgRoomsList::read(DS::Stream* stream)
{
    NetMessage::read(stream);

    m_rooms.resize(stream->read<uint32_t>());
    for (size_t i=0; i<m_rooms.size(); ++i) {
        m_rooms[i].m_location.read(stream);
        m_rooms[i].m_name = stream->readPString<uint16_t>();
    }
}

void MOUL::NetMsgRoomsList::write(DS::Stream* stream)
{
    NetMessage::write(stream);

    stream->write<uint32_t>(m_rooms.size());
    for (size_t i=0; i<m_rooms.size(); ++i) {
        m_rooms[i].m_location.write(stream);
        stream->writePString<uint16_t>(m_rooms[i].m_name);
    }
}

void MOUL::NetMsgPagingRoom::read(DS::Stream* stream)
{
    NetMsgRoomsList::read(stream);
    m_pagingFlags = stream->read<uint8_t>();
}

void MOUL::NetMsgPagingRoom::write(DS::Stream* stream)
{
    NetMsgRoomsList::write(stream);
    stream->write<uint8_t>(m_pagingFlags);
}
