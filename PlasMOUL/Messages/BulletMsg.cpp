/******************************************************************************
 * This file is part of dirtsand.                                             *
 *                                                                            *
 * dirtsand is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation, either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * dirtsand is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.          *
 ******************************************************************************/

#include "BulletMsg.h"

void MOUL::BulletMsg::read(DS::Stream* stream)
{
    Message::read(stream);

    m_cmd = stream->read<uint8_t>();
    m_from = stream->read<DS::Vector3>();
    m_direction = stream->read<DS::Vector3>();
    m_range = stream->read<float>();
    m_radius = stream->read<float>();
    m_partyTime = stream->read<float>();
}

void MOUL::BulletMsg::write(DS::Stream* stream)
{
    Message::write(stream);

    stream->write<uint8_t>(m_cmd);
    stream->write<DS::Vector3>(m_from);
    stream->write<DS::Vector3>(m_direction);
    stream->write<float>(m_range);
    stream->write<float>(m_radius);
    stream->write<float>(m_partyTime);
}
