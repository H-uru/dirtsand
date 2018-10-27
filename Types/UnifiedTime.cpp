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

#include "UnifiedTime.h"
#include <sys/time.h>

void DS::UnifiedTime::read(DS::Stream* stream)
{
    m_secs = stream->read<uint32_t>();
    m_micros = stream->read<uint32_t>();
}

void DS::UnifiedTime::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_secs);
    stream->write<uint32_t>(m_micros);
}

void DS::UnifiedTime::setNow()
{
    timeval now;
    gettimeofday(&now, nullptr);
    m_secs = now.tv_sec;
    m_micros = now.tv_usec;
}
