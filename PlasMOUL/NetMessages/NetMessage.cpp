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

#include "NetMessage.h"
#include "errors.h"

void MOUL::NetMessage::read(DS::Stream* stream)
{
    m_contentFlags = stream->read<uint32_t>();

    if (m_contentFlags & e_HasVersion) {
        m_protocolVerMaj = stream->read<uint8_t>();
        m_protocolVerMin = stream->read<uint8_t>();

        DS_PASSERT(m_protocolVerMaj == NETMSG_PROTOCOL_MAJ);
        DS_PASSERT(m_protocolVerMin == NETMSG_PROTOCOL_MIN);
    }

    if (m_contentFlags & e_HasTimeSent)
        m_timestamp.read(stream);
    if (m_contentFlags & e_HasContext)
        m_context = stream->read<uint32_t>();
    if (m_contentFlags & e_HasTransactionID)
        m_transId = stream->read<uint32_t>();
    if (m_contentFlags & e_HasPlayerID)
        m_playerId = stream->read<uint32_t>();
    if (m_contentFlags & e_HasAcctUuid)
        m_acctId.read(stream);
}

void MOUL::NetMessage::write(DS::Stream* stream)
{
    stream->write<uint32_t>(m_contentFlags);

    if (m_contentFlags & e_HasVersion) {
        stream->write<uint8_t>(m_protocolVerMaj);
        stream->write<uint8_t>(m_protocolVerMin);
    }

    if (m_contentFlags & e_HasTimeSent)
        m_timestamp.write(stream);
    if (m_contentFlags & e_HasContext)
        stream->write<uint32_t>(m_context);
    if (m_contentFlags & e_HasTransactionID)
        stream->write<uint32_t>(m_transId);
    if (m_contentFlags & e_HasPlayerID)
        stream->write<uint32_t>(m_playerId);
    if (m_contentFlags & e_HasAcctUuid)
        m_acctId.write(stream);
}
