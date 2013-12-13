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

#include "InputEventMsg.h"

void MOUL::InputEventMsg::read(DS::Stream* stream)
{
    MOUL::Message::read(stream);
    m_event = stream->read<int32_t>();
}

void MOUL::InputEventMsg::write(DS::Stream* stream) const
{
    MOUL::Message::write(stream);
    stream->write<int32_t>(m_event);
}

void MOUL::ControlEventMsg::read(DS::Stream* stream)
{
    MOUL::InputEventMsg::read(stream);
    m_controlCode = stream->read<int32_t>();
    m_activated = static_cast<bool>(stream->read<uint32_t>());
    m_controlPercent = stream->read<float>();
    m_turnToPoint = stream->read<DS::Vector3>();
    m_cmd = stream->readPString<uint16_t>();
}

void MOUL::ControlEventMsg::write(DS::Stream* stream) const
{
    MOUL::InputEventMsg::write(stream);
    stream->write<int32_t>(m_controlCode);
    stream->write<uint32_t>(static_cast<uint32_t>(m_activated));
    stream->write<float>(m_controlPercent);
    stream->write<DS::Vector3>(m_turnToPoint);
    stream->writePString<uint16_t>(m_cmd);
}
