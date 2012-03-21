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

#ifndef _DS_UNIFIEDTIME_H
#define _DS_UNIFIEDTIME_H

#include "streams.h"
#include <time.h>

namespace DS
{
    class UnifiedTime
    {
    public:
        UnifiedTime() : m_secs(0), m_micros(0) { }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

        bool isNull() const { return m_secs == 0 && m_micros == 0; }

        bool operator==(const UnifiedTime& other) const
        {
            return m_secs == other.m_secs && m_micros == other.m_micros;
        }
        bool operator!=(const UnifiedTime& other) const { return !operator==(other); }

        bool operator>(const UnifiedTime& other) const
        {
            if (m_secs > other.m_secs) {
                return true;
            } else if (m_secs == other.m_secs) {
                return m_micros > other.m_micros;
            } else {
                return false;
            }
        }

        void setNow();

    public:
        union {
            uint32_t m_secs;
            time_t m_time;
        };
        uint32_t m_micros;
    };
}

#endif
