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

#include "SetNetGroupIdMsg.h"

void MOUL::SetNetGroupIdMsg::read(DS::Stream* stream)
{
    MOUL::Message::read(stream);
    m_group.m_location.read(stream);
    m_group.m_flags = stream->read<uint8_t>();
}

void MOUL::SetNetGroupIdMsg::write(DS::Stream* stream)
{
    MOUL::Message::write(stream);
    m_group.m_location.write(stream);
    stream->write<uint8_t>(m_group.m_flags);
}


