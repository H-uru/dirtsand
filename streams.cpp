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

#include "streams.h"

DS::String DS::Stream::readString(size_t length, DS::StringType format)
{
    String result;
    if (format == e_StringUTF16) {
        chr16_t* buffer = new chr16_t[length];
        readBytes(buffer, length * sizeof(chr16_t));
        result = String::FromUtf16(buffer, length);
        delete[] buffer;
    } else {
        chr8_t* buffer = new chr8_t[length];
        readBytes(buffer, length * sizeof(chr8_t));
        result = (format == e_StringUTF8) ? String::FromUtf8(buffer, length)
                                          : String::FromRaw(buffer, length);
        delete[] buffer;
    }
    return result;
}

DS::String DS::Stream::readSafeString(DS::StringType format)
{
    uint16_t length = read<uint16_t>();
    if (!(length & 0xF000))
        read<uint16_t>();   // Discarded
    length &= 0x0FFF;

    String result;
    if (format == e_StringUTF16) {
        chr16_t* buffer = new chr16_t[length];
        readBytes(buffer, length * sizeof(chr16_t));
        if (length && (buffer[0] & 0x8000)) {
            for (uint16_t i=0; i<length; ++i)
                buffer[i] = ~buffer[i];
        }
        result = String::FromUtf16(buffer, length);
        delete[] buffer;
    } else {
        chr8_t* buffer = new chr8_t[length];
        readBytes(buffer, length * sizeof(chr8_t));
        if (length && (buffer[0] & 0x80)) {
            for (uint16_t i=0; i<length; ++i)
                buffer[i] = ~buffer[i];
        }
        result = (format == e_StringUTF8) ? String::FromUtf8(buffer, length)
                                          : String::FromRaw(buffer, length);
        delete[] buffer;
    }
    return result;
}

void DS::Stream::writeString(const String& value, DS::StringType format)
{
    if (format == e_StringUTF16) {
        StringBuffer<chr16_t> buffer = value.toUtf16();
        writeBytes(buffer.data(), buffer.length());
    } else {
        StringBuffer<chr8_t> buffer = (format == e_StringUTF8) ? value.toUtf8()
                                                               : value.toRaw();
        writeBytes(buffer.data(), buffer.length());
    }
}

void DS::Stream::writeSafeString(const String& value, DS::StringType format)
{
    if (format == e_StringUTF16) {
        StringBuffer<chr16_t> buffer = value.toUtf16();
        uint16_t length = value.length() & 0x0FFF;
        chr16_t* data = new chr16_t[length];
        memcpy(data, buffer.data(), length * sizeof(chr16_t));
        for (uint16_t i=0; i<length; ++i)
            data[i] = ~data[i];
        write<uint16_t>(length | 0xF000);
        writeBytes(data, length * sizeof(chr16_t));
        delete[] data;
    } else {
        StringBuffer<chr8_t> buffer = (format == e_StringUTF8) ? value.toUtf8()
                                                               : value.toRaw();
        uint16_t length = value.length() & 0x0FFF;
        chr8_t* data = new chr8_t[length];
        memcpy(data, buffer.data(), length * sizeof(chr8_t));
        for (uint16_t i=0; i<length; ++i)
            data[i] = ~data[i];
        write<uint16_t>(length | 0xF000);
        writeBytes(data, length * sizeof(chr8_t));
        delete[] data;
    }
}
