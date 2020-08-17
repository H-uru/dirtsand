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

#include "Uuid.h"
#include "errors.h"
#include <string_theory/format>
#include <cstdlib>

DS::Uuid::Uuid(const char* struuid)
{
    char hexbuf[9];
    if (strlen(struuid) != 36 || struuid[8] != '-' || struuid[13] != '-'
            || struuid[18] != '-' || struuid[23] != '-') {
        ST::printf(stderr, "Invalid UUID string '{}'\n", struuid);
        throw DS::MalformedData();
    }

    /* First segment */
    memcpy(hexbuf, struuid, 8);
    hexbuf[8] = 0;
    m_data1 = strtoul(hexbuf, nullptr, 16);

    /* Second segment */
    memcpy(hexbuf, struuid + 9, 4);
    hexbuf[4] = 0;
    m_data2 = strtoul(hexbuf, nullptr, 16);

    /* Third segment */
    memcpy(hexbuf, struuid + 14, 4);
    hexbuf[4] = 0;
    m_data3 = strtoul(hexbuf, nullptr, 16);

    /* Fourth segment */
    memcpy(hexbuf, struuid + 19, 2);
    hexbuf[2] = 0;
    m_data4[0] = strtoul(hexbuf, nullptr, 16);
    memcpy(hexbuf, struuid + 21, 2);
    hexbuf[2] = 0;
    m_data4[1] = strtoul(hexbuf, nullptr, 16);

    /* Fifth segment */
    memcpy(hexbuf, struuid + 24, 2);
    hexbuf[2] = 0;
    m_data4[2] = strtoul(hexbuf, nullptr, 16);
    memcpy(hexbuf, struuid + 26, 2);
    hexbuf[2] = 0;
    m_data4[3] = strtoul(hexbuf, nullptr, 16);
    memcpy(hexbuf, struuid + 28, 2);
    hexbuf[2] = 0;
    m_data4[4] = strtoul(hexbuf, nullptr, 16);
    memcpy(hexbuf, struuid + 30, 2);
    hexbuf[2] = 0;
    m_data4[5] = strtoul(hexbuf, nullptr, 16);
    memcpy(hexbuf, struuid + 32, 2);
    hexbuf[2] = 0;
    m_data4[6] = strtoul(hexbuf, nullptr, 16);
    memcpy(hexbuf, struuid + 34, 2);
    hexbuf[2] = 0;
    m_data4[7] = strtoul(hexbuf, nullptr, 16);
}

void DS::Uuid::read(DS::Stream* stream)
{
    m_data1 = stream->read<uint32_t>();
    m_data2 = stream->read<uint16_t>();
    m_data3 = stream->read<uint16_t>();
    stream->readBytes(m_data4, sizeof(m_data4));
}

void DS::Uuid::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_data1);
    stream->write<uint16_t>(m_data2);
    stream->write<uint16_t>(m_data3);
    stream->writeBytes(m_data4, sizeof(m_data4));
}

ST::string DS::Uuid::toString(bool pretty) const
{
    const ST::string uuidStr =
        ST::format("{08x}-{04x}-{04x}-{02x}{02x}-{02x}{02x}{02x}{02x}{02x}{02x}",
                   m_data1, m_data2, m_data3, m_data4[0], m_data4[1],
                   m_data4[2], m_data4[3], m_data4[4], m_data4[5],
                   m_data4[6], m_data4[7]);

    if (pretty)
        return ST_LITERAL("{") + uuidStr + ST_LITERAL("}");
    else
        return uuidStr;
}
