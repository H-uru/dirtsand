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

#include "streams.h"
#include "errors.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

DS::String DS::Stream::readString(size_t length, DS::StringType format)
{
    String result;
    if (format == e_StringUTF16) {
        chr16_t* buffer = new chr16_t[length];
        ssize_t bytes = readBytes(buffer, length * sizeof(chr16_t));
        DS_DASSERT(bytes == static_cast<ssize_t>(length * sizeof(chr16_t)));
        result = String::FromUtf16(buffer, length);
        delete[] buffer;
    } else {
        chr8_t* buffer = new chr8_t[length];
        ssize_t bytes = readBytes(buffer, length * sizeof(chr8_t));
        DS_DASSERT(bytes == static_cast<ssize_t>(length * sizeof(chr8_t)));
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
        ssize_t bytes = readBytes(buffer, length * sizeof(chr16_t));
        read<chr16_t>(); // redundant L'\0'
        DS_DASSERT(bytes == static_cast<ssize_t>(length * sizeof(chr16_t)));
        if (length && (buffer[0] & 0x8000)) {
            for (uint16_t i=0; i<length; ++i)
                buffer[i] = ~buffer[i];
        }
        result = String::FromUtf16(buffer, length);
        delete[] buffer;
    } else {
        chr8_t* buffer = new chr8_t[length];
        ssize_t bytes = readBytes(buffer, length * sizeof(chr8_t));
        DS_DASSERT(bytes == static_cast<ssize_t>(length * sizeof(chr8_t)));
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
        write<chr16_t>(0);
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

void DS::FileStream::open(const char* filename, const char* mode)
{
    close();
    m_file = fopen(filename, mode);
    if (!m_file)
        throw FileIOException(strerror(errno));
}

uint32_t DS::FileStream::size()
{
    struct stat statbuf;
    fstat(fileno(m_file), &statbuf);
    return static_cast<uint32_t>(statbuf.st_size);
}

bool DS::FileStream::atEof()
{
    int ch = fgetc(m_file);
    fputc(ch, m_file);
    return (ch == EOF);
}


ssize_t DS::BufferStream::readBytes(void* buffer, size_t count)
{
    if (m_position + count > m_size)
        count = m_size - m_position;
    memcpy(buffer, m_buffer + m_position, count);
    m_position += count;
    return count;
}

ssize_t DS::BufferStream::writeBytes(const void* buffer, size_t count)
{
    if (m_position + count > m_alloc) {
        // Resize stream
        size_t bigger = m_alloc ? m_alloc : 4096;
        while (m_position + count > bigger)
            bigger *= 2;
        uint8_t* newbuffer = new uint8_t[bigger];
        memcpy(newbuffer, m_buffer, m_size);
        delete[] m_buffer;
        m_buffer = newbuffer;
        m_alloc = bigger;
    }
    memcpy(m_buffer + m_position, buffer, count);
    m_position += count;
    if (m_position > m_size)
        m_size = m_position;
    return count;
}

void DS::BufferStream::seek(int32_t offset, int whence)
{
    if (whence == SEEK_SET)
        m_position = offset;
    else if (whence == SEEK_CUR)
        m_position += offset;
    else if (whence == SEEK_END)
        m_position = m_size - offset;

    DS_PASSERT(static_cast<int32_t>(m_position) >= 0 && m_position <= m_size);
}

void DS::BufferStream::set(const void* data, size_t size)
{
    delete[] m_buffer;
    if (data) {
        m_size = m_alloc = size;
        m_buffer = new uint8_t[m_alloc];
        memcpy(m_buffer, data, m_size);
    } else {
        m_buffer = 0;
        m_size = m_alloc = 0;
    }
    m_position = 0;
}

void DS::BufferStream::steal(uint8_t* buffer, size_t size)
{
    delete[] m_buffer;
    m_buffer = buffer;
    m_size = m_alloc = size;
    m_position = 0;
}


ssize_t DS::BlobStream::readBytes(void* buffer, size_t count)
{
    if (m_position + count > m_blob.size())
        count = m_blob.size() - m_position;
    memcpy(buffer, m_blob.buffer() + m_position, count);
    m_position += count;
    return count;
}

ssize_t DS::BlobStream::writeBytes(const void* buffer, size_t count)
{
    throw FileIOException("Cannot write to read-only stream");
}

void DS::BlobStream::seek(int32_t offset, int whence)
{
    if (whence == SEEK_SET)
        m_position = offset;
    else if (whence == SEEK_CUR)
        m_position += offset;
    else if (whence == SEEK_END)
        m_position = m_blob.size() - offset;

    DS_PASSERT(static_cast<int32_t>(m_position) >= 0 && m_position <= m_blob.size());
}
