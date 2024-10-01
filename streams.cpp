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

bool DS::Stream::readLine(void* buffer, size_t count)
{
    DS_ASSERT(count >= 1);
    char* outp = reinterpret_cast<char*>(buffer);
    char* endp = outp + count - 1;

    while (outp < endp) {
        ssize_t nread = readBytes(outp, 1);
        if (nread == 0) {
            break;
        }
        char c = *outp++;
        if (c == '\n')
            break;
    }
    *outp = 0;
    // Signal EOF if the loop terminated without reading anything into the buffer.
    return outp != buffer;
}

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
                               : value.to_latin_1();
        writeBytes(buffer.data(), buffer.size() * sizeof(char));
    }
}

void DS::Stream::writeSafeString(const ST::string& value, DS::StringType format)
{
    if (format == e_StringUTF16) {
        ST::utf16_buffer buffer = value.to_utf16();
        uint16_t length = buffer.size() & 0x0FFF;
        for (uint16_t i=0; i<length; ++i)
            buffer[i] = ~buffer[i];
        write<uint16_t>(length | 0xF000);
        writeBytes(buffer.data(), length * sizeof(char16_t));
        write<char16_t>(0);
    } else {
        ST::char_buffer buffer = (format == e_StringUTF8) ? value.to_utf8()
                               : value.to_latin_1();
        uint16_t length = buffer.size() & 0x0FFF;
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
    ungetc(ch, m_file);
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

DS::EncryptedStream::EncryptedStream(
    DS::Stream* base, DS::EncryptedStream::Mode mode,
    std::optional<DS::EncryptedStream::Type> type, const uint32_t* keys
) : m_base(base), m_buffer(), m_key(), m_pos(), m_size(),
    m_type(type.has_value() ? type.value() : DS::EncryptedStream::Type::e_tea),
    m_mode(mode)
{
    DS_ASSERT(base != nullptr);
    DS_ASSERT(base->tell() == 0);

    static constexpr uint32_t kTeaKey[] {
        0x6c0a5452,
        0x03827d0f,
        0x3a170b92,
        0x16db7fc2
    };
    if (keys) {
        memcpy(m_key, keys, sizeof(m_key));
    } else {
        static_assert(sizeof(kTeaKey) == sizeof(m_key));
        memcpy(m_key, kTeaKey, sizeof(m_key));
    }

    uint8_t header[16]{};
    DS_ASSERT(!(!type.has_value() && mode == Mode::e_write));
    switch (mode) {
    case Mode::e_read:
        base->readBytes(header, sizeof(header));
        if (memcmp(header, "whatdoyousee", 12) == 0 || memcmp(header, "BriceIsSmart", 12) == 0)
            m_type = DS::EncryptedStream::Type::e_tea;
        else if (memcmp(header, "notthedroids", 12) == 0)
            m_type = DS::EncryptedStream::Type::e_xxtea;
        else
            throw DS::FileIOException("Unknown EncryptedString magic");
        DS_ASSERT(!type.has_value() || type.value() == m_type);
        m_size = reinterpret_cast<uint32_t*>(header)[3];
        break;

    case Mode::e_write:
        // Write out some temporary junk for the header for now.
        uint8_t header[16]{};
        base->writeBytes(header, sizeof(header));
        break;
    }
}

DS::EncryptedStream::~EncryptedStream()
{
    close();
}

void DS::EncryptedStream::close()
{
    if (m_base == nullptr)
        return;

    if (m_mode == Mode::e_write) {
        if (m_pos % sizeof(m_buffer) != 0)
            cryptFlush();
        m_base->seek(0, SEEK_SET);
        switch (m_type) {
            case Type::e_xxtea:
                m_base->writeBytes("notthedroids", 12);
                break;
            case Type::e_tea:
                m_base->writeBytes("whatdoyousee", 12);
                break;
        }
        m_base->write<uint32_t>(m_size);
    }

    m_base = nullptr;
}

std::optional<DS::EncryptedStream::Type> DS::EncryptedStream::CheckEncryption(const char* filename)
{
    DS::FileStream stream;
    stream.open(filename, "r");
    return CheckEncryption(&stream);
}

std::optional<DS::EncryptedStream::Type> DS::EncryptedStream::CheckEncryption(DS::Stream* stream)
{
    uint32_t pos = stream->tell();
    if (pos != 0)
        stream->seek(0, SEEK_SET);
    uint8_t header[12];
    stream->readBytes(header, sizeof(header));
    stream->seek(pos, SEEK_SET);

    if (memcmp(header, "whatdoyousee", sizeof(header)) == 0 || memcmp(header, "BriceIsSmart", sizeof(header)) == 0)
        return DS::EncryptedStream::Type::e_tea;
    if (memcmp(header, "notthedroids", sizeof(header)) == 0)
        return DS::EncryptedStream::Type::e_xxtea;
    return std::nullopt;
}

void DS::EncryptedStream::setKeys(const uint32_t* keys)
{
    memcpy(m_key, keys, sizeof(m_key));
}

void DS::EncryptedStream::xxteaDecipher(uint32_t* buf, uint32_t num) const
{
    uint32_t key = ((52 / num) + 6) * 0x9E3779B9;
    while (key != 0) {
        uint32_t xorkey = (key >> 2) & 3;
        uint32_t numloop = num - 1;
        while (numloop != 0) {
            buf[numloop] -=
              (((buf[numloop - 1] << 4) ^ (buf[numloop - 1] >> 3)) +
              ((buf[numloop - 1] >> 5) ^ (buf[numloop - 1] << 2))) ^
              ((m_key[(numloop & 3) ^ xorkey] ^ buf[numloop - 1]) +
              (key ^ buf[numloop - 1]));
            numloop--;
        }
        buf[0] -=
          (((buf[num - 1] << 4) ^ (buf[num - 1] >> 3)) +
          ((buf[num - 1] >> 5) ^ (buf[num - 1] << 2))) ^
          ((m_key[(numloop & 3) ^ xorkey] ^ buf[num - 1]) +
          (key ^ buf[num - 1]));
        key += 0x61C88647;
    }
}

void DS::EncryptedStream::xxteaEncipher(uint32_t* buf, uint32_t num) const
{
    uint32_t key = 0;
    uint32_t count = (52 / num) + 6;
    while (count != 0) {
        key -= 0x61C88647;
        uint32_t xorkey = (key >> 2) & 3;
        uint32_t numloop = 0;
        while (numloop != num - 1) {
            buf[numloop] +=
              (((buf[numloop + 1] << 4) ^ (buf[numloop + 1] >> 3)) +
              ((buf[numloop + 1] >> 5) ^ (buf[numloop + 1] << 2))) ^
              ((m_key[(numloop & 3) ^ xorkey] ^ buf[numloop + 1]) +
              (key ^ buf[numloop + 1]));
            numloop++;
        }
        buf[num - 1] +=
          (((buf[0] << 4) ^ (buf[0] >> 3)) +
          ((buf[0] >> 5) ^ (buf[0] << 2))) ^
          ((m_key[(numloop & 3) ^ xorkey] ^ buf[0]) +
          (key ^ buf[0]));
        count--;
    }
}

void DS::EncryptedStream::teaDecipher(uint32_t* buf) const
{
    uint32_t second = buf[1], first = buf[0], key = 0xC6EF3720;

    for (size_t i = 0; i < 32; i++) {
        second -= (((first >> 5) ^ (first << 4)) + first)
                ^ (m_key[(key >> 11) & 3] + key);
        key -= 0x9E3779B9;
        first -= (((second >> 5) ^ (second << 4)) + second)
               ^ (m_key[key & 3] + key);
    }
    buf[0] = first;
    buf[1] = second;
}

void DS::EncryptedStream::teaEncipher(uint32_t* buf) const
{
    uint32_t first = buf[0], second = buf[1], key = 0;

    for (size_t i = 0; i < 32; i++) {
        first += (((second >> 5) ^ (second << 4)) + second)
               ^ (m_key[key & 3] + key);
        key += 0x9E3779B9;
        second += (((first >> 5) ^ (first << 4)) + first)
                ^ (m_key[(key >> 11) & 3] + key);
    }
    buf[1] = second;
    buf[0] = first;
}

void DS::EncryptedStream::cryptFlush()
{
    switch (m_type) {
        case Type::e_xxtea:
            xxteaEncipher(reinterpret_cast<uint32_t*>(m_buffer), 2);
            break;
        case Type::e_tea:
            teaEncipher(reinterpret_cast<uint32_t*>(m_buffer));
            break;
    }
    m_base->writeBytes(m_buffer, sizeof(m_buffer));
    memset(m_buffer, 0, sizeof(m_buffer));
}

ssize_t DS::EncryptedStream::readBytes(void* buffer, size_t count)
{
    if (m_mode != Mode::e_read)
        throw FileIOException("EncryptedStream instance is not readable");

    size_t bp = 0;
    size_t lp = m_pos % sizeof(m_buffer);
    while (bp < count) {
        if (lp == 0) {
            m_base->readBytes(m_buffer, sizeof(m_buffer));
            switch (m_type) {
                case Type::e_xxtea:
                    xxteaDecipher(reinterpret_cast<uint32_t*>(m_buffer), 2);
                    break;
                case Type::e_tea:
                    teaDecipher(reinterpret_cast<uint32_t*>(m_buffer));
                    break;
            }
        }
        if (lp + (count - bp) >= sizeof(m_buffer)) {
            memcpy(reinterpret_cast<uint8_t*>(buffer) + bp, m_buffer + lp, sizeof(m_buffer) - lp);
            bp += sizeof(m_buffer) - lp;
            lp = 0;
        } else {
            memcpy(reinterpret_cast<uint8_t*>(buffer) + bp, m_buffer + lp, count - bp);
            bp = count;
        }
    }

    m_pos += count;
    return count;
}

ssize_t DS::EncryptedStream::writeBytes(const void* buffer, size_t count)
{
    if (m_mode != Mode::e_write)
        throw DS::FileIOException("EncryptedStream instance is not writeable");

    size_t bp = 0;
    size_t lp = m_pos % sizeof(m_buffer);
    while (bp < count) {
        if (lp + (count - bp) >= sizeof(m_buffer)) {
            memcpy(m_buffer + lp, reinterpret_cast<const uint8_t*>(buffer) + bp, sizeof(m_buffer) - lp);
            bp += sizeof(m_buffer) - lp;
            cryptFlush();
            lp = 0;
        } else {
            memcpy(m_buffer + lp, reinterpret_cast<const uint8_t*>(buffer) + bp, count - bp);
            bp = count;
        }
    }

    m_pos += count;
    m_size = std::max(m_size, m_pos);
    return count;
}
