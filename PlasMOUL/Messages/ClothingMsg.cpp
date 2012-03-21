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

#include "ClothingMsg.h"

void MOUL::ClothingMsg::read(DS::Stream* stream)
{
    Message::read(stream);

    m_commands = stream->read<uint32_t>();
    if (stream->read<bool>())
        m_item.read(stream);
    m_color.m_R = stream->read<float>();
    m_color.m_G = stream->read<float>();
    m_color.m_B = stream->read<float>();
    m_color.m_A = stream->read<float>();
    m_layer = stream->read<uint8_t>();
    m_delta = stream->read<uint8_t>();
    m_weight = stream->read<float>();
}

void MOUL::ClothingMsg::write(DS::Stream* stream)
{
    Message::write(stream);

    stream->write<uint32_t>(m_commands);
    if (!m_item.isNull()) {
        stream->write<bool>(true);
        m_item.write(stream);
    } else {
        stream->write<bool>(false);
    }
    stream->write<float>(m_color.m_R);
    stream->write<float>(m_color.m_G);
    stream->write<float>(m_color.m_B);
    stream->write<float>(m_color.m_A);
    stream->write<uint8_t>(m_layer);
    stream->write<uint8_t>(m_delta);
    stream->write<float>(m_weight);
}
