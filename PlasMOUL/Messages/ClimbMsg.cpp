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

#include "ClimbMsg.h"

void MOUL::ClimbMsg::read(DS::Stream* stream)
{
    Message::read(stream);
    m_cmd = stream->read<uint32_t>();
    m_direction = stream->read<uint32_t>();
    m_status = stream->read<bool>();
    m_target.read(stream);
}

void MOUL::ClimbMsg::write(DS::Stream* stream) const
{
    Message::write(stream);
    stream->write<uint32_t>(m_cmd);
    stream->write<uint32_t>(m_direction);
    stream->write<bool>(m_status);
    m_target.write(stream);
}
