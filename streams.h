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

#ifndef _DS_STREAMS_H
#define _DS_STREAMS_H

#include <string_theory/string>
#include <atomic>
#include <stdexcept>
#include <cstdio>
#include <cstring>

namespace DS
{
    class EofException : public std::runtime_error
    {
    public:
        EofException()
            : std::runtime_error("[EofException] Unexpected end of stream") { }
    };

    class FileIOException : public std::runtime_error
    {
    public:
        explicit FileIOException(const char* msg)
            : std::runtime_error(std::string("[FileIOException] ") + msg) { }
    };

    enum StringType
    {
        // Type of string to encode/decode from stream sources
        e_StringRAW8, e_StringUTF8, e_StringUTF16,
    };

    class Stream
    {
    public:
        Stream() { }
        virtual ~Stream() { }

        virtual ssize_t readBytes(void* buffer, size_t count) = 0;
        virtual ssize_t writeBytes(const void* buffer, size_t count) = 0;

        template <typename tp, typename stream_type = tp> tp read()
        {
            stream_type value;
            if (readBytes(&value, sizeof(value)) != sizeof(value))
                throw EofException();
            return static_cast<tp>(value);
        }

        ST::string readString(size_t length, DS::StringType format = e_StringRAW8);
        ST::string readSafeString(DS::StringType format = e_StringRAW8);

        template <typename tp, typename stream_type = tp> void write(tp value)
        {
            auto svalue = static_cast<stream_type>(value);
            writeBytes(&svalue, sizeof(svalue));
        }

        template <typename sz_t>
        ST::string readPString(DS::StringType format = e_StringRAW8)
        {
            sz_t length = read<sz_t>();
            return readString(length);
        }

        template <typename sz_t>
        void writePString(const ST::string& value, DS::StringType format = e_StringRAW8)
        {
            if (format == e_StringUTF16) {
                ST::utf16_buffer buffer = value.to_utf16();
                write<sz_t>(buffer.size());
                writeBytes(buffer.data(), buffer.size() * sizeof(char16_t));
            } else {
                ST::char_buffer buffer = (format == e_StringUTF8) ? value.to_utf8()
                                       : value.to_latin_1(ST::substitute_invalid);
                write<sz_t>(buffer.size());
                writeBytes(buffer.data(), buffer.size() * sizeof(char));
            }
        }

        void writeString(const ST::string& value, DS::StringType format = e_StringRAW8);
        void writeSafeString(const ST::string& value, DS::StringType format = e_StringRAW8);

        virtual uint32_t tell() const = 0;
        virtual void seek(int32_t offset, int whence) = 0;
        virtual uint32_t size() const = 0;
        virtual bool atEof() = 0;
        virtual void flush() = 0;
    };

    // Special cases for bool
    template <> inline bool Stream::read<bool>()
    { return read<uint8_t>() != 0; }

    template <> inline void Stream::write<bool>(bool value)
    { write<uint8_t>(value ? 1 : 0); }


    /* Stream for reading/writing regular files */
    class FileStream : public Stream
    {
    public:
        FileStream() : m_file() { }
        ~FileStream() override { close(); }

        void open(const char* filename, const char* mode);
        void close()
        {
            if (m_file)
                fclose(m_file);
            m_file = nullptr;
        }

        ssize_t readBytes(void* buffer, size_t count) override
        { return fread(buffer, 1, count, m_file); }

        ssize_t writeBytes(const void* buffer, size_t count) override
        { return fwrite(buffer, 1, count, m_file); }

        uint32_t tell() const override { return static_cast<uint32_t>(ftell(m_file)); }
        void seek(int32_t offset, int whence) override;
        uint32_t size() const override;
        bool atEof() override;
        void flush() override { fflush(m_file); }

    private:
        FILE* m_file;
    };

    /* Read/Write RAM stream */
    class BufferStream : public Stream
    {
    public:
        BufferStream() : m_buffer(), m_position(), m_size(), m_alloc(), m_refs(1) { }
        BufferStream(const void* data, size_t size) : m_buffer(), m_refs(1) { set(data, size); }
        ~BufferStream() override { delete[] m_buffer; }

        ssize_t readBytes(void* buffer, size_t count) override;
        ssize_t writeBytes(const void* buffer, size_t count) override;

        uint32_t tell() const override { return static_cast<uint32_t>(m_position); }
        void seek(int32_t offset, int whence) override;
        uint32_t size() const override { return m_size; }
        bool atEof() override { return m_position >= m_size; }
        void flush() override { }

        void truncate()
        {
            m_size = 0;
            m_position = 0;
        }

        const uint8_t* buffer() const { return m_buffer; }

        void set(const void* buffer, size_t size);
        void steal(uint8_t* buffer, size_t size);

        void ref() { ++m_refs; }
        void unref()
        {
            if (--m_refs == 0)
                delete this;
        }

    private:
        uint8_t* m_buffer;
        size_t m_position;
        size_t m_size, m_alloc;
        std::atomic_int m_refs;

        BufferStream(const BufferStream& copy) { }
        void operator=(const BufferStream& copy) { }
    };

    /* Read-only non-copyable RAM stream */
    class Blob
    {
    public:
        Blob() noexcept : m_buffer(), m_size() { }

        Blob(const uint8_t* buffer, size_t size)
        {
            auto bufcopy = new uint8_t[size];
            memcpy(bufcopy, buffer, size);
            m_buffer = const_cast<const uint8_t*>(bufcopy);
            m_size = size;
        }

        Blob(Blob&& other) noexcept
            : m_buffer(other.m_buffer), m_size(other.m_size)
        {
            other.m_buffer = nullptr;
        }

        ~Blob() noexcept { delete[] m_buffer; }

        Blob(const Blob&) = delete;
        Blob& operator=(const Blob&) = delete;

        Blob& operator=(Blob&& other) noexcept
        {
            m_buffer = other.m_buffer;
            m_size = other.m_size;
            other.m_buffer = nullptr;
            return *this;
        }

        static Blob Steal(const uint8_t* buffer, size_t size)
        {
            Blob b;
            b.m_buffer = buffer;
            b.m_size = size;
            return b;
        }

        template <size_t length>
        static Blob FromString(const char (&text)[length])
        {
            // Subtract one character for the nul-terminator
            return Blob(reinterpret_cast<const uint8_t*>(text), length - 1);
        }

        const uint8_t* buffer() const { return m_buffer; }
        size_t size() const { return m_size; }

        Blob copy() const
        {
            return Blob(buffer(), size());
        }

    private:
        const uint8_t* m_buffer;
        size_t m_size;
    };

    class BlobStream : public Stream
    {
    public:
        explicit BlobStream(const Blob& blob) : m_blob(blob), m_position(0) { }
        explicit BlobStream(Blob&&) = delete;   // Prevent dangling references
        ~BlobStream() override { }

        ssize_t readBytes(void* buffer, size_t count) override;
        ssize_t writeBytes(const void* buffer, size_t count) override;

        uint32_t tell() const override { return static_cast<uint32_t>(m_position); }
        void seek(int32_t offset, int whence) override;
        uint32_t size() const override { return static_cast<uint32_t>(m_blob.size()); }
        bool atEof() override { return tell() >= size(); }
        void flush() override { }

    private:
        const Blob& m_blob;
        size_t m_position;
    };

    Blob Base64Decode(const ST::string& value);
    Blob HexDecode(const ST::string& value);
}

#endif
