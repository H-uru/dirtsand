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

#include "NetMsgSharedState.h"
#include "errors.h"

void MOUL::NetMsgSharedState::read(DS::Stream* stream)
{
    NetMsgObject::read(stream);

    NetMsgStream msgStream;
    msgStream.read(stream);
    m_compression = msgStream.m_compression;
    //TODO

    m_lockRequest = stream->read<uint8_t>();
}

void MOUL::NetMsgSharedState::write(DS::Stream* stream)
{
    NetMsgObject::write(stream);

    //TODO
    DS_PASSERT(0);

    stream->write<uint8_t>(m_lockRequest);
}
