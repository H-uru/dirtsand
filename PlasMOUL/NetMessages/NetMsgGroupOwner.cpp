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

#include "NetMsgGroupOwner.h"

MOUL::NetGroupId MOUL::NetGroupId::Unknown
        (Location(0xFF000001, Location::e_Reserved), 0);
MOUL::NetGroupId MOUL::NetGroupId::LocalPlayer
        (Location(0xFF000002, Location::e_Reserved), e_NetGroupConstant);
MOUL::NetGroupId MOUL::NetGroupId::RemotePlayer
        (Location(0xFF000003, Location::e_Reserved), e_NetGroupConstant);
MOUL::NetGroupId MOUL::NetGroupId::LocalPhysicals
        (Location(0xFF000004, Location::e_Reserved), e_NetGroupConstant);
MOUL::NetGroupId MOUL::NetGroupId::RemotePhysicals
        (Location(0xFF000005, Location::e_Reserved), e_NetGroupConstant);

void MOUL::NetMsgGroupOwner::read(DS::Stream* stream)
{
    NetMessage::read(stream);

    m_groups.resize(stream->read<uint32_t>());
    for (size_t i=0; i<m_groups.size(); ++i) {
        m_groups[i].m_group.m_location.read(stream);
        m_groups[i].m_group.m_flags = stream->read<uint8_t>();
        m_groups[i].m_own = stream->read<bool>();
    }
}

void MOUL::NetMsgGroupOwner::write(DS::Stream* stream)
{
    NetMessage::write(stream);

    stream->write<uint32_t>(m_groups.size());
    for (size_t i=0; i<m_groups.size(); ++i) {
        m_groups[i].m_group.m_location.write(stream);
        stream->write<uint8_t>(m_groups[i].m_group.m_flags);
        stream->write<bool>(m_groups[i].m_own);
    }
}
