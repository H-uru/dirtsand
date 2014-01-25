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

#include "Math.h"
#include "streams.h"

bool DS::Matrix44::operator==(const DS::Matrix44& other) const
{
    if (m_identity && other.m_identity)
        return true;
    return memcmp(m_map, other.m_map, sizeof(m_map)) == 0;
}

void DS::Matrix44::reset()
{
    static const float s_identity[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                                           {0.0f, 1.0f, 0.0f, 0.0f},
                                           {0.0f, 0.0f, 1.0f, 0.0f},
                                           {0.0f, 0.0f, 0.0f, 1.0f}};
    memcpy(m_map, s_identity, sizeof(m_map));
    m_identity = true;
}

void DS::Matrix44::read(DS::Stream* stream)
{
    m_identity = !stream->read<bool>();
    if (!m_identity)
    {
        for (uint8_t i = 0; i < 4; i++)
            for (uint8_t j = 0; j < 4; j++)
                m_map[i][j] = stream->read<float>();
    }
    else
    {
        reset();
    }
}

void DS::Matrix44::write(DS::Stream* stream) const
{
    stream->write<bool>(!m_identity);
    if (!m_identity)
    {
        for (uint8_t i = 0; i < 4; i++)
            for (uint8_t j = 0; j < 4; j++)
                stream->write<float>(m_map[i][j]);
    }
}
