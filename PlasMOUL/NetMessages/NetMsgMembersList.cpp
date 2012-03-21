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

#include "NetMsgMembersList.h"

void MOUL::ClientGuid::read(DS::Stream* stream)
{
    m_flags = stream->read<uint16_t>();

    if (m_flags & e_HasAcctUuid)
        m_AcctUuid.read(stream);
    if ((m_flags & e_HasPlayerId) || (m_flags & e_HasTempPlayerId))
        m_PlayerId = stream->read<uint32_t>();
    if (m_flags & e_HasPlayerName)
        m_PlayerName = stream->readPString<uint16_t>();
    if (m_flags & e_HasCCRLevel)
        m_CCRLevel = stream->read<uint8_t>();
    if (m_flags & e_HasProtectedLogin)
        m_ProtectedLogin = stream->read<bool>();
    if (m_flags & e_HasBuildType)
        m_BuildType = stream->read<uint8_t>();
    if (m_flags & e_HasSrcAddr)
        m_SrcAddr = stream->read<uint32_t>();
    if (m_flags & e_HasSrcPort)
        m_SrcPort = stream->read<uint16_t>();
    if (m_flags & e_HasReserved)
        m_Reserved = stream->read<uint16_t>();
    if (m_flags & e_HasClientKey)
        m_ClientKey = stream->readPString<uint16_t>();
}

void MOUL::ClientGuid::write(DS::Stream* stream)
{
    stream->write<uint16_t>(m_flags);

    if (m_flags & e_HasAcctUuid)
        m_AcctUuid.write(stream);
    if ((m_flags & e_HasPlayerId) || (m_flags & e_HasTempPlayerId))
        stream->write<uint32_t>(m_PlayerId);
    if (m_flags & e_HasPlayerName) {
        DS::StringBuffer<chr8_t> asciibuf = m_PlayerName.toRaw();
        stream->write<uint16_t>(asciibuf.length());
        stream->writeBytes(asciibuf.data(), asciibuf.length());
    }
    if (m_flags & e_HasCCRLevel)
        stream->write<uint8_t>(m_CCRLevel);
    if (m_flags & e_HasProtectedLogin)
        stream->write<bool>(m_ProtectedLogin);
    if (m_flags & e_HasBuildType)
        stream->write<uint8_t>(m_BuildType);
    if (m_flags & e_HasSrcAddr)
        stream->write<uint32_t>(m_SrcAddr);
    if (m_flags & e_HasSrcPort)
        stream->write<uint16_t>(m_SrcPort);
    if (m_flags & e_HasReserved)
        stream->write<uint16_t>(m_Reserved);
    if (m_flags & e_HasClientKey) {
        DS::StringBuffer<chr8_t> asciibuf = m_ClientKey.toRaw();
        stream->write<uint16_t>(asciibuf.length());
        stream->writeBytes(asciibuf.data(), asciibuf.length());
    }
}

void MOUL::NetMsgMemberInfo::read(DS::Stream* stream)
{
    m_flags = stream->read<uint32_t>();
    m_client.read(stream);
    m_avatarKey.read(stream);
}

void MOUL::NetMsgMemberInfo::write(DS::Stream* stream)
{
    stream->write<uint32_t>(m_flags);
    m_client.write(stream);
    m_avatarKey.write(stream);
}

void MOUL::NetMsgMembersList::read(DS::Stream* stream)
{
    NetMessage::read(stream);

    m_members.resize(stream->read<uint16_t>());
    for (size_t i=0; i<m_members.size(); ++i)
        m_members[i].read(stream);
}

void MOUL::NetMsgMembersList::write(DS::Stream* stream)
{
    NetMessage::write(stream);

    stream->write<uint16_t>(m_members.size());
    for (size_t i=0; i<m_members.size(); ++i)
        m_members[i].write(stream);
}

void MOUL::NetMsgMemberUpdate::read(DS::Stream* stream)
{
    NetMessage::read(stream);

    m_member.read(stream);
    m_addMember = stream->read<bool>();
}

void MOUL::NetMsgMemberUpdate::write(DS::Stream* stream)
{
    NetMessage::write(stream);

    m_member.write(stream);
    stream->write<bool>(m_addMember);
}
