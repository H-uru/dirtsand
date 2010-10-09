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

#include "strings.h"

template <typename char_type>
static size_t tstrlen(const char_type* string)
{
    const char_type* sptr = string;
    while (*sptr)
        ++sptr;
    return sptr - string;
}

static void raw_to_utf8(chr8_t** dest, size_t* destlen,
                        const chr8_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const chr8_t* sptr = src;
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
        *dest = new chr8_t[convlen + 1];
        chr8_t* dptr = *dest;
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
        dest[convlen] = 0;
    }
}

static void utf16_to_utf8(chr8_t** dest, size_t* destlen,
                          const chr16_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const chr16_t* sptr = src;
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
        *dest = new chr8_t[convlen + 1];
        chr8_t* dptr = *dest;
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
                ++sptr;
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
        dest[convlen] = 0;
    }
}

static void utf8_to_raw(chr8_t** dest, size_t* destlen,
                        const chr8_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const chr8_t* sptr = src;
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
        *dest = new chr8_t[convlen + 1];
        chr8_t* dptr = *dest;
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
                    ch += (*sptr++ & 0x3f);
                *dptr++ = (ch <= 0xFF) ? static_cast<chr8_t>(ch) : '?';
            } else {
                *dptr++ = *sptr++;
            }
        }
        (*dest)[convlen] = 0;
    }
}

static void utf8_to_utf16(chr16_t** dest, size_t* destlen,
                          const chr8_t* src, ssize_t srclen)
{
    if (srclen < 0)
        srclen = static_cast<size_t>(tstrlen(src));

    size_t convlen = 0;
    const chr8_t* sptr = src;
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
        *dest = new chr16_t[convlen + 1];
        chr16_t* dptr = *dest;
        sptr = src;
        while (sptr < src + srclen) {
            if ((*sptr & 0xF8) == 0xF0) {
                int ch  = (*sptr++ & 0x1F) << 18;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3f) << 12;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3f) << 6;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3f);
                *dptr++ = 0xD800 | ((ch >> 10) & 0x3FF);
                *dptr++ = 0xDC00 | ((ch      ) & 0x3FF);
            } else if ((*sptr & 0xF0) == 0xE0) {
                int ch  = (*sptr++ & 0x1F) << 12;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3f) << 6;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3f);
                *dptr++ = ch;
            } else if ((*sptr & 0xE0) == 0xC0) {
                int ch  = (*sptr++ & 0x1F) << 6;
                if (sptr < src + srclen)
                    ch += (*sptr++ & 0x3f);
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
        chr8_t* bufdata = new chr8_t[length + 1];
        memcpy(bufdata, strconst, length);
        bufdata[length] = 0;
        m_data = StringBuffer<chr8_t>(bufdata, length);
    }
    return *this;
}

DS::String& DS::String::operator+=(const char* strconst)
{
    if (isEmpty())
        return operator=(strconst);

    if (strconst && *strconst != 0) {
        size_t addlen = tstrlen(strconst);
        chr8_t* buffer = new chr8_t[m_data.m_buffer->m_length + addlen + 1];
        memcpy(buffer, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
        memcpy(buffer + m_data.m_buffer->m_length, strconst, addlen);
        buffer[m_data.m_buffer->m_length + addlen] = 0;
        m_data = StringBuffer<chr8_t>(buffer, m_data.m_buffer->m_length + addlen);
    }
    return *this;
}

DS::String& DS::String::operator+=(const String& other)
{
    if (isEmpty())
        return operator=(other);

    if (!other.isEmpty()) {
        chr8_t* buffer = new chr8_t[m_data.m_buffer->m_length + other.m_data.m_buffer->m_length + 1];
        memcpy(buffer, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
        memcpy(buffer + m_data.m_buffer->m_length, other.m_data.m_buffer->m_string, other.m_data.m_buffer->m_length);
        buffer[m_data.m_buffer->m_length + other.m_data.m_buffer->m_length] = 0;
        m_data = StringBuffer<chr8_t>(buffer, m_data.m_buffer->m_length + other.m_data.m_buffer->m_length);
    }
    return *this;
}

int DS::String::compare(const char* strconst, CaseSensitivity cs)
{
    if (isEmpty())
        return (!strconst || *strconst == 0) ? 0 : -1;
    if (!strconst || *strconst == 0)
        return 1;
    if (cs == e_CaseSensitive)
        return strcmp(reinterpret_cast<const char*>(m_data.m_buffer->m_string), strconst);
    else
        return strcasecmp(reinterpret_cast<const char*>(m_data.m_buffer->m_string), strconst);
}

int DS::String::compare(const String& other, CaseSensitivity cs)
{
    if (isEmpty())
        return other.isEmpty() ? 0 : -1;
    if (other.isEmpty())
        return 1;
    if (cs == e_CaseSensitive)
        return strcmp(reinterpret_cast<const char*>(m_data.m_buffer->m_string),
                      reinterpret_cast<const char*>(other.m_data.m_buffer->m_string));
    else
        return strcasecmp(reinterpret_cast<const char*>(m_data.m_buffer->m_string),
                          reinterpret_cast<const char*>(other.m_data.m_buffer->m_string));
}

DS::StringBuffer<chr8_t> DS::String::toRaw() const
{
    if (isNull())
        return StringBuffer<chr8_t>();

    chr8_t* buffer;
    size_t length;
    utf8_to_raw(&buffer, &length, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
    return StringBuffer<chr8_t>(buffer, length);
}

DS::StringBuffer<chr8_t> DS::String::toUtf8() const
{
    /* Provide a deep copy so the original string doesn't affect this buffer. */
    if (isNull())
        return StringBuffer<chr8_t>();

    chr8_t* buffer = new chr8_t[m_data.m_buffer->m_length + 1];
    memcpy(buffer, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
    buffer[m_data.m_buffer->m_length] = 0;
    return StringBuffer<chr8_t>(buffer, m_data.m_buffer->m_length);
}

DS::StringBuffer<chr16_t> DS::String::toUtf16() const
{
    if (isNull())
        return StringBuffer<chr16_t>();

    chr16_t* buffer;
    size_t length;
    utf8_to_utf16(&buffer, &length, m_data.m_buffer->m_string, m_data.m_buffer->m_length);
    return StringBuffer<chr16_t>(buffer, length);
}

DS::String DS::String::FromRaw(const chr8_t* string, ssize_t length)
{
    if (!string)
        return String();

    chr8_t* buffer;
    size_t buflen;
    raw_to_utf8(&buffer, &buflen, string, length);
    String result;
    result.m_data = StringBuffer<chr8_t>(buffer, buflen);
    return result;
}

DS::String DS::String::FromUtf8(const chr8_t* string, ssize_t length)
{
    if (!string)
        return String();

    if (length < 0)
        length = tstrlen(string);
    chr8_t* buffer = new chr8_t[length + 1];
    memcpy(buffer, string, length);
    buffer[length] = 0;
    String result;
    result.m_data = StringBuffer<chr8_t>(buffer, length);
    return result;
}

DS::String DS::String::FromUtf16(const chr16_t* string, ssize_t length)
{
    if (!string)
        return String();

    chr8_t* buffer;
    size_t buflen;
    utf16_to_utf8(&buffer, &buflen, string, length);
    String result;
    result.m_data = StringBuffer<chr8_t>(buffer, buflen);
    return result;
}
