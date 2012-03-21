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

#include "NetMsgVoice.h"

void MOUL::NetMsgVoice::read(DS::Stream* stream)
{
    NetMessage::read(stream);

    m_flags = stream->read<uint8_t>();
    m_frames = stream->read<uint8_t>();

    size_t length = stream->read<uint16_t>();
    uint8_t* buffer = new uint8_t[length];
    stream->readBytes(buffer, length);
    m_data = DS::Blob::Steal(buffer, length);

    m_receivers.resize(stream->read<uint8_t>());
    for (size_t i=0; i<m_receivers.size(); ++i)
        m_receivers[i] = stream->read<uint32_t>();
}

void MOUL::NetMsgVoice::write(DS::Stream* stream)
{
    NetMessage::write(stream);

    stream->write<uint8_t>(m_flags);
    stream->write<uint8_t>(m_frames);

    stream->write<uint16_t>(m_data.size());
    stream->writeBytes(m_data.buffer(), m_data.size());

    stream->write<uint8_t>(m_receivers.size());
    for (size_t i=0; i<m_receivers.size(); ++i)
        stream->write<uint32_t>(m_receivers[i]);
}
