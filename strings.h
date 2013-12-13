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

#ifndef _DS_STRINGS_H
#define _DS_STRINGS_H

#include "config.h"
#include <vector>
#include <cstring>
#include <cstdarg>
#include <unordered_map>
#include <string>
#ifdef HAVE_ATOMIC
  #include <atomic>
#else
  #include <stdatomic.h>
#endif

/* Important note:  Strings passed in as raw character constants (e.g. "blah")
 * are assumed to be in UTF-8 format, for the sake of minimal conversion
 * complexity.  The safest bet, however, is simply to stick to 7-bit strings
 * in character constants, or to use the conversion functions when necessary.
 */

namespace DS
{
    enum StringType
    {
        e_StringRAW8, e_StringUTF8, e_StringUTF16,
    };

    enum CaseSensitivity
    {
        e_CaseSensitive, e_CaseInsensitive,
    };

    template <typename char_type>
    class StringBuffer
    {
    public:
        StringBuffer() : m_buffer(0) { }

        StringBuffer(const StringBuffer<char_type>& copy)
            : m_buffer(copy.m_buffer)
        {
            if (m_buffer)
                ++m_buffer->m_refs;
        }

        ~StringBuffer<char_type>()
        {
            if (m_buffer && --m_buffer->m_refs == 0)
                delete m_buffer;
        }

        StringBuffer<char_type>& operator=(const StringBuffer<char_type>& copy)
        {
            if (copy.m_buffer)
                ++copy.m_buffer->m_refs;
            if (m_buffer && --m_buffer->m_refs == 0)
                delete m_buffer;
            m_buffer = copy.m_buffer;
            return *this;
        }

        size_t length() const { return m_buffer ? m_buffer->m_length : 0; }
        const char_type* data() const { return m_buffer ? m_buffer->m_string : 0; }
        bool isNull() const { return m_buffer == 0; }
        bool isEmpty() const { return length() == 0; }

    private:
        struct _buffer
        {
            const char_type* m_string;
            size_t m_length;
            std::atomic_int m_refs;

            _buffer(const char_type* str, size_t length)
                : m_string(str), m_length(length), m_refs(1) { }

            ~_buffer() { delete[] m_string; }
        }* m_buffer;

        /* Constructor for DS::String */
        StringBuffer(const char_type* stringData, size_t length)
            : m_buffer(new _buffer(stringData, length)) { }
        friend class String;
    };

    class String
    {
    public:
        String() { }
        String(const char* strconst) { operator=(strconst); }
        String(const String& copy) : m_data(copy.m_data) { }
        virtual ~String() { }

        String& operator=(const char* strconst);
        String& operator=(const String& copy)
        {
            m_data = copy.m_data;
            return *this;
        }

        bool operator==(const char* strconst) const { return compare(strconst) == 0; }
        bool operator==(const String& other) const { return compare(other) == 0; }
        bool operator!=(const char* strconst) const { return !operator==(strconst); }
        bool operator!=(const String& other) const { return !operator==(other); }
        String& operator+=(const char* strconst);
        String& operator+=(const String& other);
        String operator+(const char* strconst) const { return String(*this) += strconst; }
        String operator+(const String& other) const { return String(*this) += other; }

        int compare(const char* strconst, CaseSensitivity cs = e_CaseSensitive) const;
        int compare(const String& other, CaseSensitivity cs = e_CaseSensitive) const;

        size_t length() const { return m_data.length(); }
        const char* c_str() const { return reinterpret_cast<const char*>(m_data.data()); }
        bool isNull() const { return m_data.isNull(); }
        bool isEmpty() const { return m_data.isEmpty(); }

        /* Conversion */
        StringBuffer<chr8_t> toRaw() const;
        StringBuffer<chr8_t> toUtf8() const;
        StringBuffer<chr16_t> toUtf16() const;
        static String FromRaw(const chr8_t* string, ssize_t length = -1);
        static String FromUtf8(const chr8_t* string, ssize_t length = -1);
        static String FromUtf16(const chr16_t* string, ssize_t length = -1);

        String toUpper() const;
        String toLower() const;
        int32_t toInt(int base = 0) const;
        uint32_t toUint(int base = 0) const;
        float toFloat() const;
        double toDouble() const;
        bool toBool() const;

        /* Manipulation */
        std::vector<String> split(char separator = 0, ssize_t max = -1) const;
        String left(ssize_t length) const;
        String right(ssize_t length) const;
        String mid(size_t start, ssize_t length = -1) const;
        String strip(char comment = 0) const;

        ssize_t find(const char* substr, ssize_t start = 0) const;
        ssize_t rfind(const char* substr, ssize_t start = -1) const;
        void replace(const char* from, const char* to);

        /* Creation */
        static String Format(const char* fmt, ...);
        static String FormatV(const char* fmt, va_list aptr);
        static String Steal(const char* buffer, ssize_t length = -1);

    private:
        StringBuffer<chr8_t> m_data;
    };

    struct StringHash : public std::hash<std::string>
    {
        size_t operator()(const String& value) const
        {
            return std::hash<std::string>::operator()(value.c_str());
        }
    };
}

#endif
