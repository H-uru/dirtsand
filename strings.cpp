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

#include "strings.h"
#include "errors.h"
#include <list>
#include <cstdio>
#include <ctype.h>

template <typename char_type>
static size_t tstrlen(const char_type* string)
{
    const char_type* sptr = string;
    while (*sptr)
        ++sptr;
    return sptr - string;
}

static void raw_to_utf8(uint8_t** dest, size_t* destlen,
                        const uint8_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const uint8_t* sptr = src;
    while (sptr < src + srclen) {
        if (*sptr >= 0x80)
            convlen += 2;
        else
            convlen += 1;
        ++sptr;
    }
    if (destlen)
        *destlen = convlen;

    if (dest) {
        *dest = new uint8_t[convlen + 1];
        uint8_t* dptr = *dest;
        sptr = src;
        while (sptr < src + srclen) {
            if (*sptr >= 0x80) {
                *dptr++ = 0xC0 | ((*sptr >> 6) & 0x1F);
                *dptr++ = 0x80 | ((*sptr     ) & 0x3F);
            } else {
                *dptr++ = *sptr;
            }
            ++sptr;
        }
        (*dest)[convlen] = 0;
    }
}

static void utf16_to_utf8(uint8_t** dest, size_t* destlen,
                          const uint16_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const uint16_t* sptr = src;
    while (sptr < src + srclen) {
        if (*sptr >= 0xD800 && *sptr <= 0xDFFF) {
            convlen += 4;
            ++sptr;
        } else if (*sptr >= 0x800) {
            convlen += 3;
        } else if (*sptr >= 0x80) {
            convlen += 2;
        } else {
            convlen += 1;
        }
        ++sptr;
    }
    if (destlen)
        *destlen = convlen;

    if (dest) {
        *dest = new uint8_t[convlen + 1];
        uint8_t* dptr = *dest;
        sptr = src;
        while (sptr < src + srclen) {
            if (*sptr >= 0xD800 && *sptr <= 0xDFFF) {
                uint32_t ch = 0x10000;
                if (sptr + 1 >= src + srclen) {
                    /* Incomplete surrogate pair */
                    *dptr++ = 0;
                    break;
                } else if (*sptr < 0xDC00) {
                    ch += (*sptr & 0x3FF) << 10;
                    ++sptr;
                    ch += (*sptr & 0x3FF);
                } else {
                    ch += (*sptr & 0x3FF);
                    ++sptr;
                    ch += (*sptr & 0x3FF) << 10;
                }
                *dptr++ = 0xF0 | ((ch >> 18) & 0x07);
                *dptr++ = 0x80 | ((ch >> 12) & 0x3F);
                *dptr++ = 0x80 | ((ch >>  6) & 0x3F);
                *dptr++ = 0x80 | ((ch      ) & 0x3F);
            } else if (*sptr >= 0x800) {
                *dptr++ = 0xE0 | ((*sptr >> 12) & 0x0F);
                *dptr++ = 0x80 | ((*sptr >>  6) & 0x3F);
                *dptr++ = 0x80 | ((*sptr      ) & 0x3F);
            } else if (*sptr >= 0x80) {
                *dptr++ = 0xC0 | ((*sptr >> 6) & 0x1F);
                *dptr++ = 0x80 | ((*sptr     ) & 0x3F);
            } else {
                *dptr++ = *sptr;
            }
            ++sptr;
        }
        (*dest)[convlen] = 0;
    }
}

static void utf8_to_raw(uint8_t** dest, size_t* destlen,
                        const uint8_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const uint8_t* sptr = src;
    while (sptr < src + srclen) {
        if ((*sptr & 0xF8) == 0xF0)
            sptr += 4;
        else if ((*sptr & 0xF0) == 0xE0)
            sptr += 3;
        else if ((*sptr & 0xE0) == 0xC0)
            sptr += 2;
        else
            sptr += 1;
        ++convlen;
    }
    if (destlen)
        *destlen = convlen;

    if (dest) {
        *dest = new uint8_t[convlen + 1];
        uint8_t* dptr = *dest;
        sptr = src;
        while (sptr < src + srclen) {
            if ((*sptr & 0xF8) == 0xF0) {
                *dptr++ = '?';
                sptr += 4;
            } else if ((*sptr & 0xF0) == 0xE0) {
                *dptr++ = '?';
                sptr += 3;
            } else if ((*sptr & 0xE0) == 0xC0) {
                int ch  = (*sptr++ & 0x1F) << 6;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3F);
                *dptr++ = (ch <= 0xFF) ? static_cast<uint8_t>(ch) : '?';
            } else {
                *dptr++ = *sptr++;
            }
        }
        (*dest)[convlen] = 0;
    }
}

static void utf8_to_utf16(uint16_t** dest, size_t* destlen,
                          const uint8_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const uint8_t* sptr = src;
    while (sptr < src + srclen) {
        if ((*sptr & 0xF8) == 0xF0) {
            /* Surrogate pair needed */
            ++convlen;
            sptr += 4;
        } else if ((*sptr & 0xF0) == 0xE0) {
            sptr += 3;
        } else if ((*sptr & 0xE0) == 0xC0) {
            sptr += 2;
        } else {
            sptr += 1;
        }
        ++convlen;
    }
    if (destlen)
        *destlen = convlen;

    if (dest) {
        *dest = new uint16_t[convlen + 1];
        uint16_t* dptr = *dest;
        sptr = src;
        while (sptr < src + srclen) {
            if ((*sptr & 0xF8) == 0xF0) {
                int ch  = (*sptr++ & 0x07) << 18;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3F) << 12;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3F) << 6;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3F);
                *dptr++ = 0xD800 | ((ch >> 10) & 0x3FF);
                *dptr++ = 0xDC00 | ((ch      ) & 0x3FF);
            } else if ((*sptr & 0xF0) == 0xE0) {
                int ch  = (*sptr++ & 0x0F) << 12;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3F) << 6;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3F);
                *dptr++ = ch;
            } else if ((*sptr & 0xE0) == 0xC0) {
                int ch  = (*sptr++ & 0x1F) << 6;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3F);
                *dptr++ = ch;
            } else {
                *dptr++ = *sptr++;
            }
        }
        (*dest)[convlen] = 0;
    }
}


DS::String& DS::String::operator=(const char* strconst)
{
    if (strconst) {
        size_t length = tstrlen(strconst);
        char* bufdata = new char[length + 1];
        memcpy(bufdata, strconst, length);
        bufdata[length] = 0;
        m_data = StringBuffer<char>(bufdata, length);
    }
    return *this;
}

DS::String& DS::String::operator+=(const char* strconst)
{
    if (isEmpty())
        return operator=(strconst);

    if (strconst && *strconst != 0) {
        size_t addlen = tstrlen(strconst);
        char* buffer = new char[m_data.m_buffer->m_length + addlen + 1];
        memcpy(buffer, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
        memcpy(buffer + m_data.m_buffer->m_length, strconst, addlen);
        buffer[m_data.m_buffer->m_length + addlen] = 0;
        m_data = StringBuffer<char>(buffer, m_data.m_buffer->m_length + addlen);
    }
    return *this;
}

DS::String& DS::String::operator+=(const String& other)
{
    if (isEmpty())
        return operator=(other);

    if (!other.isEmpty()) {
        char* buffer = new char[m_data.m_buffer->m_length + other.m_data.m_buffer->m_length + 1];
        memcpy(buffer, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
        memcpy(buffer + m_data.m_buffer->m_length, other.m_data.m_buffer->m_string, other.m_data.m_buffer->m_length);
        buffer[m_data.m_buffer->m_length + other.m_data.m_buffer->m_length] = 0;
        m_data = StringBuffer<char>(buffer, m_data.m_buffer->m_length + other.m_data.m_buffer->m_length);
    }
    return *this;
}

int DS::String::compare(const char* strconst, CaseSensitivity cs) const
{
    if (isEmpty())
        return (!strconst || *strconst == 0) ? 0 : -1;
    if (!strconst || *strconst == 0)
        return 1;
    if (cs == e_CaseSensitive)
        return strcmp(m_data.m_buffer->m_string, strconst);
    else
        return strcasecmp(m_data.m_buffer->m_string, strconst);
}

int DS::String::compare(const String& other, CaseSensitivity cs) const
{
    if (isEmpty())
        return other.isEmpty() ? 0 : -1;
    if (other.isEmpty())
        return 1;
    if (cs == e_CaseSensitive)
        return strcmp(m_data.m_buffer->m_string, other.m_data.m_buffer->m_string);
    else
        return strcasecmp(m_data.m_buffer->m_string, other.m_data.m_buffer->m_string);
}

DS::StringBuffer<char> DS::String::toRaw() const
{
    if (isNull())
        return StringBuffer<char>();

    uint8_t* buffer;
    size_t length;
    utf8_to_raw(&buffer, &length, reinterpret_cast<const uint8_t*>(m_data.m_buffer->m_string),
                m_data.m_buffer->m_length);
    return StringBuffer<char>(reinterpret_cast<char*>(buffer), length);
}

DS::StringBuffer<char> DS::String::toUtf8() const
{
    /* Provide a deep copy so the original string doesn't affect this buffer. */
    if (isNull())
        return StringBuffer<char>();

    char* buffer = new char[m_data.m_buffer->m_length + 1];
    memcpy(buffer, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
    buffer[m_data.m_buffer->m_length] = 0;
    return StringBuffer<char>(buffer, m_data.m_buffer->m_length);
}

DS::StringBuffer<char16_t> DS::String::toUtf16() const
{
    if (isNull())
        return StringBuffer<char16_t>();

    uint16_t* buffer;
    size_t length;
    utf8_to_utf16(&buffer, &length, reinterpret_cast<const uint8_t*>(m_data.m_buffer->m_string),
                  m_data.m_buffer->m_length);
    return StringBuffer<char16_t>(reinterpret_cast<char16_t*>(buffer), length);
}

DS::String DS::String::FromRaw(const char* string, ssize_t length)
{
    if (!string)
        return String();

    uint8_t* buffer;
    size_t buflen;
    raw_to_utf8(&buffer, &buflen, reinterpret_cast<const uint8_t*>(string), length);
    String result;
    result.m_data = StringBuffer<char>(reinterpret_cast<char*>(buffer), buflen);
    return result;
}

DS::String DS::String::FromUtf8(const char* string, ssize_t length)
{
    if (!string)
        return String();

    if (length < 0)
        length = tstrlen(string);
    char* buffer = new char[length + 1];
    memcpy(buffer, string, length);
    buffer[length] = 0;
    String result;
    result.m_data = StringBuffer<char>(buffer, length);
    return result;
}

DS::String DS::String::FromUtf16(const char16_t* string, ssize_t length)
{
    if (!string)
        return String();

    uint8_t* buffer;
    size_t buflen;
    utf16_to_utf8(&buffer, &buflen, reinterpret_cast<const uint16_t*>(string), length);
    String result;
    result.m_data = StringBuffer<char>(reinterpret_cast<char*>(buffer), buflen);
    return result;
}

int32_t DS::String::toInt(int base) const
{
    if (isEmpty())
        return 0;
    return static_cast<int32_t>(strtol(c_str(), 0, base));
}

uint32_t DS::String::toUint(int base) const
{
    if (isEmpty())
        return 0;
    return static_cast<uint32_t>(strtoul(c_str(), 0, base));
}

float DS::String::toFloat() const
{
    if (isEmpty())
        return 0;
    return strtof(c_str(), 0);
}

double DS::String::toDouble() const
{
    if (isEmpty())
        return 0;
    return strtod(c_str(), 0);
}

bool DS::String::toBool() const
{
    if (isEmpty())
        return false;
    if (compare("true", e_CaseInsensitive) == 0)
        return true;
    return toInt() != 0;
}

std::vector<DS::String> DS::String::split(char separator, ssize_t max) const
{
    if (isEmpty())
        return std::vector<DS::String>();

    std::list<DS::String> subs;
    const char* cptr = m_data.data();
    const char* scanp = cptr;
    while (*scanp && max) {
        if (!separator && isspace(*scanp)) {
            subs.push_back(DS::String::FromUtf8(cptr, scanp - cptr));
            --max;
            while (*scanp && isspace(*scanp))
                ++scanp;
            cptr = scanp;
        } else if (*scanp == separator) {
            subs.push_back(DS::String::FromUtf8(cptr, scanp - cptr));
            --max;
            cptr = scanp + 1;
        }
        ++scanp;
    }
    subs.push_back(DS::String::FromUtf8(cptr));
    return std::vector<DS::String>(subs.begin(), subs.end());
}

DS::String DS::String::left(ssize_t count) const
{
    if (count < 0) {
        count += length();
        if (count < 0)
            return String();
    }
    if (static_cast<size_t>(count) >= length())
        return *this;

    char* trimbuf = new char[count+1];
    memcpy(trimbuf, m_data.data(), count);
    trimbuf[count] = 0;
    String result;
    result.m_data = StringBuffer<char>(trimbuf, count);
    return result;
}

DS::String DS::String::right(ssize_t count) const
{
    if (count < 0) {
        count += length();
        if (count < 0)
            return String();
    }
    if (static_cast<size_t>(count) >= length())
        return *this;

    char* trimbuf = new char[count+1];
    memcpy(trimbuf, m_data.data() + m_data.length() - count, count);
    trimbuf[count] = 0;
    String result;
    result.m_data = StringBuffer<char>(trimbuf, count);
    return result;
}

DS::String DS::String::mid(size_t start, ssize_t count) const
{
    if (count < 0)
        count = length() - start;
    if (static_cast<size_t>(count) >= length())
        return *this;
    if (start >= length())
        return String();

    char* trimbuf = new char[count+1];
    memcpy(trimbuf, m_data.data() + start, count);
    trimbuf[count] = 0;
    String result;
    result.m_data = StringBuffer<char>(trimbuf, count);
    return result;
}

DS::String DS::String::strip(char comment) const
{
    if (isNull())
        return DS::String();

    char* strbuf = new char[m_data.length()+1];
    memcpy(strbuf, m_data.data(), m_data.length());
    strbuf[m_data.length()] = 0;

    char* startp = strbuf;
    while (isspace(*startp))
        ++startp;
    char* scanp;
    if (comment) {
        scanp = startp;
        while (*scanp) {
            if (*scanp == comment)
                *scanp = 0;
            else
                ++scanp;
        }
    }
    scanp = startp + strlen(startp);
    while (scanp > startp && isspace(*(scanp-1)))
        --scanp;
    *scanp = 0;

    DS::String result(startp);
    delete[] strbuf;
    return result;
}

ssize_t DS::String::find(const char* substr, ssize_t start) const
{
    DS_DASSERT(substr);
    DS_DASSERT(start >= 0);
    size_t sublen = strlen(substr);

    while (start + sublen <= length()) {
        if (strncmp(m_data.data() + start, substr, sublen) == 0)
            return start;
        ++start;
    }
    return -1;
}

ssize_t DS::String::rfind(const char* substr, ssize_t start) const
{
    DS_DASSERT(substr);
    size_t sublen = strlen(substr);
    if (start < 0)
        start = length() - sublen;
    DS_DASSERT(static_cast<size_t>(start) <= (length() - sublen));

    while (start >= 0) {
        if (strncmp(m_data.data() + start, substr, sublen) == 0)
            return start;
        --start;
    }
    return -1;
}

void DS::String::replace(const char* from, const char* to)
{
    ssize_t start = 0;
    size_t skiplen = strlen(from);
    String result;
    for ( ;; ) {
        ssize_t next = find(from, start);
        if (next == -1)
            break;
        result += mid(start, next - start) + to;
        start = next + skiplen;
    }
    operator=(result + mid(start));
}

DS::String DS::String::Format(const char* fmt, ... )
{
    va_list aptr;
    va_start(aptr, fmt);
    String result = FormatV(fmt, aptr);
    va_end(aptr);
    return result;
}

DS::String DS::String::FormatV(const char* fmt, va_list aptr)
{
    char buffer[256];
    va_list aptr_copy;
    va_copy(aptr_copy, aptr);
    int chars = vsnprintf(buffer, 256, fmt, aptr_copy);
    va_end(aptr_copy);
    DS_DASSERT(chars >= 0);
    if (chars >= 256) {
        char* bigbuf = new char[chars+1];
        vsnprintf(bigbuf, chars+1, fmt, aptr);
        return Steal(bigbuf);
    }
    return DS::String(buffer);
}

DS::String DS::String::Steal(const char* buffer, ssize_t length)
{
    DS::String stolen;
    if (length < 0)
        length = tstrlen(buffer);
    stolen.m_data = StringBuffer<char>(buffer, length);
    return stolen;
}
