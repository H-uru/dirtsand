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

#include "InputIfaceMgrMsg.h"

void MOUL::InputIfaceMgrMsg::read(DS::Stream* stream)
{
    Message::read(stream);

    m_command = stream->read<uint8_t>();
    m_pageId = stream->read<uint32_t>();
    m_ageName = stream->readSafeString();
    m_ageFilename = stream->readSafeString();
    m_spawnPoint = stream->readSafeString();
    m_avatar.read(stream);
}

void MOUL::InputIfaceMgrMsg::write(DS::Stream* stream) const
{
    Message::write(stream);

    stream->write<uint8_t>(m_command);
    stream->write<uint32_t>(m_pageId);
    stream->writeSafeString(m_ageName);
    stream->writeSafeString(m_ageFilename);
    stream->writeSafeString(m_spawnPoint);
    m_avatar.write(stream);
}
