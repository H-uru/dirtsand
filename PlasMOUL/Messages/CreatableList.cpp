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

#include "CreatableList.h"
#include "factory.h"
#include "errors.h"
#include <zlib.h>
#include <memory>

#define COMPRESSION_THRESHOLD 255

void MOUL::CreatableList::clear()
{
    for (auto& it : m_items)
        it.second->unref();
    m_items.clear();
}

void MOUL::CreatableList::read(DS::Stream* stream)
{
    clear();

    m_flags = stream->read<uint8_t>();

    uint32_t bufSz = stream->read<uint32_t>();
    std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSz]);

    if (m_flags & e_Compressed) {
        uint32_t zBufSz = stream->read<uint32_t>();
        std::unique_ptr<uint8_t[]> zBuf(new uint8_t[zBufSz]);
        stream->readBytes(zBuf.get(), zBufSz);
        uLongf zLen;
        int result = uncompress(buf.get(), &zLen, zBuf.get(), zBufSz);
        if (result != Z_OK || zLen != bufSz)
            throw DS::MalformedData();
        m_flags &= ~e_Compressed;
    } else {
        stream->readBytes(buf.get(), bufSz);
    }

    DS::BufferStream ram;
    ram.steal(buf.release(), bufSz);
    uint16_t numItems = ram.read<uint16_t>();
    for (uint16_t i = 0; i < numItems; i++) {
        uint16_t id = ram.read<uint16_t>();
        uint16_t type = ram.read<uint16_t>();
        Creatable* cre = Factory::Create(type);
        if (!cre)
            throw DS::MalformedData();
        cre->read(&ram);
        m_items[id] = cre;
    }
}

void MOUL::CreatableList::write(DS::Stream* stream) const
{
    DS::BufferStream ram;
    uint16_t numItems = m_items.size();
    ram.write<uint16_t>(numItems);
    for (auto& it : m_items) {
        uint16_t id = it.first;
        Creatable* item = it.second;
        uint16_t type = item->type();
        ram.write<uint16_t>(id);
        ram.write<uint16_t>(type);
        item->write(&ram);
    }

    uint32_t bufSz = ram.tell();
    ram.seek(0, SEEK_SET);
    std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSz]);
    ram.readBytes(buf.get(), bufSz);
    uLongf zBufSz;

    uint8_t flags = m_flags & ~e_Compressed;
    if (flags & e_WantCompression && bufSz > COMPRESSION_THRESHOLD) {
        std::unique_ptr<uint8_t[]> zBuf(new uint8_t[bufSz]);
        int result = compress(zBuf.get(), &zBufSz, buf.get(), bufSz);
        if (result == Z_OK) {
            memcpy(buf.get(), zBuf.get(), zBufSz);
            flags |= e_Compressed;
        }
        // If compression failed, just write the uncompressed list...
    }

    ram.truncate();
    ram.write<uint8_t>(flags);
    ram.write<uint32_t>(bufSz);

    if (flags & e_Compressed) {
        bufSz = zBufSz;
        ram.write<uint32_t>(bufSz);
    }

    ram.writeBytes(buf.get(), bufSz);
    stream->writeBytes(ram.buffer(), ram.tell());
}
