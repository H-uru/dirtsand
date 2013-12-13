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

#include "KIMessage.h"

void MOUL::KIMessage::read(DS::Stream* stream)
{
    Message::read(stream);

    m_command = stream->read<uint8_t>();
    m_user = stream->readSafeString();
    m_playerId = stream->read<uint32_t>();
    m_string = stream->readSafeString(DS::e_StringUTF16);
    m_flags = stream->read<uint32_t>();
    m_delay = stream->read<float>();
    m_value = stream->read<int32_t>();
}

void MOUL::KIMessage::write(DS::Stream* stream) const
{
    Message::write(stream);

    stream->write<uint8_t>(m_command);
    stream->writeSafeString(m_user);
    stream->write<uint32_t>(m_playerId);
    stream->writeSafeString(m_string, DS::e_StringUTF16);
    stream->write<uint32_t>(m_flags);
    stream->write<float>(m_delay);
    stream->write<int32_t>(m_value);
}

bool MOUL::KIMessage::makeSafeForNet()
{
    if (m_command != e_ChatMessage) {
        // Client is being naughty
        fprintf(stderr, "Ignoring KI message %d\n", m_command);
        return false;
    }
    m_flags &= ~e_AdminMsg;
    return true;
}
