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

#include "NetMsgSDLState.h"

void MOUL::NetMsgSDLState::read(DS::Stream* stream)
{
    NetMsgObject::read(stream);

    NetMsgStream blobStream;
    blobStream.read(stream);
    m_compression = blobStream.m_compression;
    uint8_t* sdl = new uint8_t[blobStream.m_stream.size()];
    memcpy(sdl, blobStream.m_stream.buffer(), blobStream.m_stream.size());
    m_sdlBlob = DS::Blob::Steal(sdl, blobStream.m_stream.size());

    m_isInitial = stream->read<bool>();
    m_persistOnServer = stream->read<bool>();
    m_isAvatar = stream->read<bool>();
}

void MOUL::NetMsgSDLState::write(DS::Stream* stream) const
{
    NetMsgObject::write(stream);

    NetMsgStream blobStream(m_compression);
    blobStream.m_stream.writeBytes(m_sdlBlob.buffer(), m_sdlBlob.size());
    blobStream.write(stream);

    stream->write<bool>(m_isInitial);
    stream->write<bool>(m_persistOnServer);
    stream->write<bool>(m_isAvatar);
}
