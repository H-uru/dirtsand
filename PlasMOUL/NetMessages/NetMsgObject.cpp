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

#include "NetMsgObject.h"
#include "errors.h"
#include <zlib.h>

void MOUL::NetMsgStream::read(DS::Stream* stream)
{
    uint32_t uncompressedSize = stream->read<uint32_t>();
    m_compression = static_cast<Compression>(stream->read<uint8_t>());
    uint32_t size = stream->read<uint32_t>();
    uint8_t* buffer = new uint8_t[size];
    stream->readBytes(buffer, size);

    if (m_compression == e_CompressZlib) {
        DS_PASSERT(size >= 2);

        uint8_t* zbuf = new uint8_t[uncompressedSize];
        uLongf zlength = uncompressedSize - 2;
        memcpy(zbuf, buffer, 2);
        int result = uncompress(zbuf + 2, &zlength, buffer + 2, size - 2);
        if (result != Z_OK) {
            delete[] zbuf;
            delete[] buffer;
            DS_PASSERT(0);
        }
        m_stream.steal(zbuf, uncompressedSize);
        delete[] buffer;
    } else {
        m_stream.steal(buffer, size);
    }
}

void MOUL::NetMsgStream::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_stream.size());
    stream->write<uint8_t>(m_compression);

    if (m_compression == e_CompressZlib) {
        DS_PASSERT(m_stream.size() >= 2);

        uLongf zlength = compressBound(m_stream.size() - 2);
        uint8_t* zbuf = new uint8_t[zlength + 2];
        memcpy(zbuf, m_stream.buffer(), 2);
        int result = compress(zbuf + 2, &zlength, m_stream.buffer() + 2, m_stream.size() - 2);
        if (result != Z_OK) {
            delete[] zbuf;
            DS_PASSERT(0);
        }
        stream->write<uint32_t>(zlength + 2);
        stream->writeBytes(zbuf, zlength + 2);
        delete[] zbuf;
    } else {
        stream->write<uint32_t>(m_stream.size());
        stream->writeBytes(m_stream.buffer(), m_stream.size());
    }
}

void MOUL::NetMsgObject::read(DS::Stream* stream)
{
    NetMessage::read(stream);
    m_object.read(stream);
}

void MOUL::NetMsgObject::write(DS::Stream* stream) const
{
    NetMessage::write(stream);
    m_object.write(stream);
}
