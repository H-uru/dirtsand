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

#ifndef _DS_UUID_H
#define _DS_UUID_H

#include "streams.h"

#define SET_DATA4(b0, b1, b2, b3, b4, b5, b6, b7) \
    m_data4[0] = b0; m_data4[1] = b1; m_data4[2] = b2; m_data4[3] = b3; \
    m_data4[4] = b4; m_data4[5] = b5; m_data4[6] = b6; m_data4[7] = b7

namespace DS
{
    class Uuid
    {
    public:
        Uuid() : m_data1(0), m_data2(0), m_data3(0)
        {
            SET_DATA4(0, 0, 0, 0, 0, 0, 0, 0);
        }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

    private:
        uint32_t m_data1;
        uint16_t m_data2, m_data3;
        uint8_t m_data4[8];
    };
}

#endif
