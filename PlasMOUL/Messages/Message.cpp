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

#include "Message.h"

void MOUL::Message::read(DS::Stream* stream)
{
    m_sender.read(stream);
    m_receivers.resize(stream->read<uint32_t>());
    for (size_t i=0; i<m_receivers.size(); ++i)
        m_receivers[i].read(stream);
    m_timestamp = stream->read<double>();
    m_bcastFlags = stream->read<uint32_t>();
}

void MOUL::Message::write(DS::Stream* stream) const
{
    m_sender.write(stream);
    stream->write<uint32_t>(m_receivers.size());
    for (size_t i=0; i<m_receivers.size(); ++i)
        m_receivers[i].write(stream);
    stream->write<double>(m_timestamp);
    stream->write<uint32_t>(m_bcastFlags);
}
