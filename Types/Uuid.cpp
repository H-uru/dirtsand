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

#include "Uuid.h"
#include "errors.h"
#include <cstring>

DS::Uuid::Uuid(const char* struuid)
{
    DS_DASSERT(strlen(struuid) == 36);
    DS_DASSERT(struuid[8] == '-' && struuid[13] == '-' && struuid[18] == '-'
               && struuid[23] == '-');
    m_data1 = strtoul(struuid, 0, 16);
}

void DS::Uuid::read(DS::Stream* stream)
{
    m_data1 = stream->read<uint32_t>();
    m_data2 = stream->read<uint16_t>();
    m_data3 = stream->read<uint16_t>();
    stream->readBytes(m_data4, sizeof(m_data4));
}

void DS::Uuid::write(DS::Stream* stream)
{
    stream->write<uint32_t>(m_data1);
    stream->write<uint16_t>(m_data2);
    stream->write<uint16_t>(m_data3);
    stream->writeBytes(m_data4, sizeof(m_data4));
}

DS::String DS::Uuid::toString() const
{
    return DS::String::Format("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                              m_data1, m_data2, m_data3, m_data4[0], m_data4[1],
                              m_data4[2], m_data4[3], m_data4[4], m_data4[5],
                              m_data4[6], m_data4[7]);
}
