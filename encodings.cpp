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

#include "encodings.h"
#include "errors.h"

static char b64_digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_codes[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};

DS::String DS::Base64Encode(const uint8_t* data, size_t length)
{
    size_t resultLen = ((length + 2) / 3) * 4;
    char* result = new char[resultLen+1];

    char* outp = result;
    const uint8_t* inp = data;
    while (length > 2) {
        outp[0] = b64_digits[inp[0] >> 2];
        outp[1] = b64_digits[((inp[0] & 0x03) << 4) | ((inp[1] & 0xF0) >> 4)];
        outp[2] = b64_digits[((inp[1] & 0x0F) << 2) | ((inp[2] & 0xC0) >> 6)];
        outp[3] = b64_digits[inp[2] & 0x3F];
        inp += 3;
        outp += 4;
        length -= 3;
    }

    /* Last few bytes */
    switch (length) {
    case 2:
        outp[0] = b64_digits[inp[0] >> 2];
        outp[1] = b64_digits[((inp[0] & 0x03) << 4) | ((inp[1] & 0xF0) >> 4)];
        outp[2] = b64_digits[((inp[1] & 0x0F) << 2)];
        outp[3] = '=';
        break;
    case 1:
        outp[0] = b64_digits[inp[0] >> 2];
        outp[1] = b64_digits[((inp[0] & 0x03) << 4)];
        outp[2] = '=';
        outp[3] = '=';
        break;
    }

    result[resultLen] = 0;
    return DS::String::Steal(result, resultLen);
}

DS::Blob* DS::Base64Decode(const DS::String& value)
{
    size_t length = value.length();
    DS_PASSERT((length % 4) == 0);
    if (length == 0)
        return new Blob(0, 0);

    size_t resultLen = (length * 3) / 4;
    if (value.length() > 0) {
        if (value.c_str()[length-1] == '=')
            --resultLen;
        if (value.c_str()[length-2] == '=')
            --resultLen;
    }
    uint8_t* result = new uint8_t[resultLen];

    uint8_t* outp = result;
    const uint8_t* inp = reinterpret_cast<const uint8_t*>(value.c_str());
    while (length > 4) {
        DS_PASSERT(inp[0] < 0x80 && b64_codes[inp[0]] >= 0);
        DS_PASSERT(inp[1] < 0x80 && b64_codes[inp[1]] >= 0);
        DS_PASSERT(inp[2] < 0x80 && b64_codes[inp[2]] >= 0);
        DS_PASSERT(inp[3] < 0x80 && b64_codes[inp[3]] >= 0);
        outp[0] = (b64_codes[inp[0]] << 2) | ((b64_codes[inp[1]] >> 4) & 0x03);
        outp[1] = ((b64_codes[inp[1]] << 4) & 0xF0) | ((b64_codes[inp[2]] >> 2) & 0x0F);
        outp[2] = ((b64_codes[inp[2]] << 6) & 0xC0) | (b64_codes[inp[3]] & 0x3F);
        inp += 4;
        outp += 3;
        length -= 4;
    }

    /* Last chunk */
    DS_PASSERT(inp[0] < 0x80 && b64_codes[inp[0]] >= 0);
    DS_PASSERT(inp[1] < 0x80 && b64_codes[inp[1]] >= 0);
    outp[0] = (b64_codes[inp[0]] << 2) | ((b64_codes[inp[1]] >> 4) & 0x03);
    if (inp[2] != '=') {
        DS_PASSERT(inp[2] < 0x80 && b64_codes[inp[2]] >= 0);
        outp[1] = ((b64_codes[inp[1]] << 4) & 0xF0) | ((b64_codes[inp[2]] >> 2) & 0x0F);
    }
    if (inp[3] != '=') {
        DS_PASSERT(inp[3] < 0x80 && b64_codes[inp[3]] >= 0);
        outp[2] = ((b64_codes[inp[2]] << 6) & 0xC0) | (b64_codes[inp[3]] & 0x3F);
    }

    return new Blob(result, resultLen);
}
