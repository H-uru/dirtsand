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
#include <string_theory/codecs>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

ST::string DS::Stream::readString(size_t length, DS::StringType format)
{
    if (format == e_StringUTF16) {
        ST::utf16_buffer result;
        result.allocate(length);
        ssize_t bytes = readBytes(result.data(), length * sizeof(char16_t));
        if (bytes != static_cast<ssize_t>(length * sizeof(char16_t)))
            throw EofException();
        return ST::string::from_utf16(result, ST::substitute_invalid);
    } else {
        ST::char_buffer result;
        result.allocate(length);
        ssize_t bytes = readBytes(result.data(), length * sizeof(char));
        if (bytes != static_cast<ssize_t>(length * sizeof(char)))
            throw EofException();
        return (format == e_StringUTF8) ? ST::string::from_utf8(result, ST::substitute_invalid)
                                        : ST::string::from_latin_1(result);
    }
}

ST::string DS::Stream::readSafeString(DS::StringType format)
{
    uint16_t length = read<uint16_t>();
    if (!(length & 0xF000))
        read<uint16_t>();   // Discarded
    length &= 0x0FFF;

    if (format == e_StringUTF16) {
        ST::utf16_buffer result;
        result.allocate(length);
        ssize_t bytes = readBytes(result.data(), length * sizeof(char16_t));
        read<char16_t>(); // redundant u'\0'
        if (bytes != static_cast<ssize_t>(length * sizeof(char16_t)))
            throw EofException();
        if ((result.front() & 0x8000) != 0) {
            for (uint16_t i=0; i<length; ++i)
                result[i] = ~result[i];
        }
        return ST::string::from_utf16(result, ST::substitute_invalid);
    } else {
        ST::char_buffer result;
        result.allocate(length);
        ssize_t bytes = readBytes(result.data(), length * sizeof(char));
        if (bytes != static_cast<ssize_t>(length * sizeof(char)))
            throw EofException();
        if ((result.front() & 0x80) != 0) {
            for (uint16_t i=0; i<length; ++i)
                result[i] = ~result[i];
        }
        return (format == e_StringUTF8)
               ? ST::string::from_utf8(result, ST::substitute_invalid)
               : ST::string::from_latin_1(result);
    }
}

void DS::Stream::writeString(const ST::string& value, DS::StringType format)
{
    if (format == e_StringUTF16) {
        ST::utf16_buffer buffer = value.to_utf16();
        writeBytes(buffer.data(), buffer.size() * sizeof(char16_t));
    } else {
        ST::char_buffer buffer = (format == e_StringUTF8) ? value.to_utf8()
                               : value.to_latin_1(ST::substitute_invalid);
        writeBytes(buffer.data(), buffer.size() * sizeof(char));
    }
}

void DS::Stream::writeSafeString(const ST::string& value, DS::StringType format)
{
    if (format == e_StringUTF16) {
        ST::utf16_buffer buffer = value.to_utf16();
        uint16_t length = value.size() & 0x0FFF;
        for (uint16_t i=0; i<length; ++i)
            buffer[i] = ~buffer[i];
        write<uint16_t>(length | 0xF000);
        writeBytes(buffer.data(), length * sizeof(char16_t));
        write<char16_t>(0);
    } else {
        ST::char_buffer buffer = (format == e_StringUTF8) ? value.to_utf8()
                               : value.to_latin_1(ST::substitute_invalid);
        uint16_t length = value.size() & 0x0FFF;
        for (uint16_t i=0; i<length; ++i)
            buffer[i] = ~buffer[i];
        write<uint16_t>(length | 0xF000);
        writeBytes(buffer.data(), length * sizeof(char));
    }
}

void DS::FileStream::open(const char* filename, const char* mode)
{
    close();
    m_file = fopen(filename, mode);
    if (!m_file)
        throw FileIOException(strerror(errno));
}

void DS::FileStream::seek(int32_t offset, int whence)
{
    if (fseek(m_file, offset, whence) < 0)
        throw FileIOException(strerror(errno));
}

uint32_t DS::FileStream::size() const
{
    struct stat statbuf;
    if (fstat(fileno(m_file), &statbuf) < 0)
        throw FileIOException(strerror(errno));
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
        if (m_size != 0)
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

    if (static_cast<int32_t>(m_position) < 0)
        m_position = 0;
    else if (m_position > m_size)
        m_position = m_size;
}

void DS::BufferStream::set(const void* data, size_t size)
{
    delete[] m_buffer;
    if (data) {
        m_size = m_alloc = size;
        m_buffer = new uint8_t[m_alloc];
        memcpy(m_buffer, data, m_size);
    } else {
        m_buffer = nullptr;
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

    if (static_cast<int32_t>(m_position) < 0)
        m_position = 0;
    else if (m_position > m_blob.size())
        m_position = m_blob.size();
}

DS::Blob DS::Base64Decode(const ST::string& value)
{
    ST_ssize_t resultLen = ST::base64_decode(value, nullptr, 0);
    if (resultLen < 0)
        throw DS::MalformedData();

    uint8_t* result = new uint8_t[resultLen];
    ST::base64_decode(value, result, resultLen);
    return Blob::Steal(result, resultLen);
}

DS::Blob DS::HexDecode(const ST::string& value)
{
    ST_ssize_t resultLen = ST::hex_decode(value, nullptr, 0);
    if (resultLen < 0)
        throw DS::MalformedData();

    uint8_t* result = new uint8_t[resultLen];
    ST::hex_decode(value, result, resultLen);
    return Blob::Steal(result, resultLen);
}
