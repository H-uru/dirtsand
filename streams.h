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

#ifndef _DS_STREAMS_H
#define _DS_STREAMS_H

#include "strings.h"

namespace DS
{
    class Stream
    {
    public:
        Stream() { }
        virtual ~Stream() { }

        virtual ssize_t readBytes(void* buffer, size_t count) = 0;
        virtual ssize_t writeBytes(const void* buffer, size_t count) = 0;

        uint8_t readByte();
        uint16_t read16();
        uint32_t read32();
        uint64_t read64();
        float readFloat();
        double readDouble();
        String readString(size_t length, DS::StringType format = e_StringRAW8);
        String readSafeString(DS::StringType format = e_StringRAW8);

        void writeByte(uint8_t value);
        void write16(uint16_t value);
        void write32(uint32_t value);
        void write64(uint64_t value);
        void writeFloat(float value);
        void writeDouble(double value);
        void writeString(const String& value, DS::StringType format = e_StringRAW8);
        void writeSafeString(const String& value, DS::StringType format = e_StringRAW8);

        virtual uint64_t tell() = 0;
        virtual void seek(uint64_t offset, int whence) = 0;
        virtual uint64_t size() = 0;
        virtual bool ateof() = 0;
        virtual void flush() = 0;
    };

    class FileStream : public Stream
    {
        //
    };

    class BufferStream : public Stream
    {
        //
    };
}

#endif
