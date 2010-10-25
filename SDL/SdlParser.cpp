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

#include "SdlParser.h"

bool SDL::Parser::open(const char* filename)
{
    char sanitycheck[12];

    m_file = fopen(filename, "r");
    if (!m_file) {
        fprintf(stderr, "[SDL] Error opening file %s for reading\n", filename);
        return false;
    }
    memset(sanitycheck, 0, sizeof(sanitycheck));
    fread(sanitycheck, 1, 12, m_file);
    fseek(m_file, 0, SEEK_SET);
    if (memcmp(sanitycheck, "whatdoyousee", 12) == 0
        || memcmp(sanitycheck, "notthedroids", 12) == 0
        || memcmp(sanitycheck, "BriceIsSmart", 12) == 0) {
        fprintf(stderr, "[SDL] Error: DirtSand does not support encrypted SDL sources\n");
        fprintf(stderr, "[SDL] Please decrypt your SDL files and re-start DirtSand\n");
        fprintf(stderr, "[SDL] Error in file: %s\n", filename);
        return false;
    }

    m_filename = filename;
    m_lineno = 0;
    return true;
}

static SDL::TokenType str_to_toktype(DS::String str)
{
    if (str == "STATEDESC")
        return SDL::e_TokStatedesc;
    if (str == "VERSION")
        return SDL::e_TokVersion;
    if (str == "VAR")
        return SDL::e_TokVar;
    if (str == "INT")
        return SDL::e_TokInt;
    if (str == "FLOAT")
        return SDL::e_TokFloat;
    if (str == "BOOL")
        return SDL::e_TokBool;
    if (str == "STRING32")
        return SDL::e_TokString;
    if (str == "PLKEY")
        return SDL::e_TokPlKey;
    if (str == "CREATABLE")
        return SDL::e_TokCreatable;
    if (str == "DOUBLE")
        return SDL::e_TokDouble;
    if (str == "TIME")
        return SDL::e_TokTime;
    if (str == "BYTE")
        return SDL::e_TokByte;
    if (str == "SHORT")
        return SDL::e_TokShort;
    if (str == "AGETIMEOFDAY")
        return SDL::e_TokAgeTimeOfDay;
    if (str == "VECTOR3")
        return SDL::e_TokVector3;
    if (str == "POINT3")
        return SDL::e_TokPoint3;
    if (str == "QUATERNION")
        return SDL::e_TokQuat;
    if (str == "RGB")
        return SDL::e_TokRgb;
    if (str == "RGB8")
        return SDL::e_TokRgb8;
    if (str == "RGBA")
        return SDL::e_TokRgba;
    if (str == "RGBA8")
        return SDL::e_TokRgba8;
    if (str == "DEFAULT")
        return SDL::e_TokDefault;
    if (str == "DEFAULTOPTION")
        return SDL::e_TokDefaultOption;
    return SDL::e_TokIdent;
}

SDL::Token SDL::Parser::next()
{
    while (m_buffer.empty()) {
        Token tokbuf;
        chr8_t lnbuf[4096];
        if (!fgets(reinterpret_cast<char*>(lnbuf), 4096, m_file)) {
            tokbuf.m_type = e_TokEof;
            m_buffer.push_back(tokbuf);
            break;
        }
        tokbuf.m_lineno = ++m_lineno;

        chr8_t* lnp = lnbuf;
        while (*lnp) {
            if (*lnp == '#') {
                // Comment - terminate scanning
                break;
            } else if (*lnp == ' ' || *lnp == '\t' || *lnp == '\b'
                       || *lnp == '\r' || *lnp == '\n') {
                // Skip whitespace
                ++lnp;
            } else if ((*lnp >= '0' && *lnp <= '9') || *lnp == '-') {
                // Numeric constant
                chr8_t* endp = lnp + 1;
                while (*endp && ((*endp >= '0' && *endp <= '9') || *endp == '.'))
                    ++endp;
                tokbuf.m_type = e_TokNumeric;
                tokbuf.m_value = DS::String::FromRaw(lnp, endp - lnp);
                m_buffer.push_back(tokbuf);
                lnp = endp;
            } else if ((*lnp >= 'A' && *lnp <= 'Z') || (*lnp >= 'a' && *lnp <= 'z')
                        || *lnp == '_') {
                // Identifier or keyword
                chr8_t* endp = lnp + 1;
                while (*endp && ((*endp >= 'A' && *endp <= 'Z') || (*endp >= 'a' && *endp <= 'z')
                       || (*endp >= '0' && *endp <= '9') || *endp == '_'))
                    ++endp;
                tokbuf.m_value = DS::String::FromRaw(lnp, endp - lnp);
                tokbuf.m_type = str_to_toktype(tokbuf.m_value);
                m_buffer.push_back(tokbuf);
                lnp = endp;
            } else if (*lnp == '$') {
                // Statedesc reference
                chr8_t* endp = lnp + 1;
                while (*endp && ((*endp >= 'A' && *endp <= 'Z') || (*endp >= 'a' && *endp <= 'z')
                       || (*endp >= '0' && *endp <= '9') || *endp == '_'))
                    ++endp;
                tokbuf.m_type = e_TokTypename;
                tokbuf.m_value = DS::String::FromRaw(lnp + 1, endp - lnp - 1);
                m_buffer.push_back(tokbuf);
                lnp = endp;
            } else if (*lnp == '(' || *lnp == ')' || *lnp == '[' || *lnp == ']'
                       || *lnp == '{' || *lnp == '}' || *lnp == '=' || *lnp == ','
                       || *lnp == ';') {
                tokbuf.m_type = static_cast<TokenType>(*lnp);
                m_buffer.push_back(tokbuf);
                ++lnp;
            } else if (*lnp == '"') {
                // String constant
                chr8_t* endp = lnp + 1;
                while (*endp && *endp != '"')
                    ++endp;
                tokbuf.m_type = e_TokQuoted;
                tokbuf.m_value = DS::String::FromRaw(lnp + 1, endp - lnp - 1);
                m_buffer.push_back(tokbuf);
                lnp = endp + 1;
            } else {
                fprintf(stderr, "[SDL] %s:%ld: Unexpected character '%c'\n",
                        m_filename.c_str(), m_lineno, *lnp);
                tokbuf.m_type = e_TokError;
                m_buffer.push_back(tokbuf);
                break;
            }
        }
    }

    Token tok = m_buffer.front();
    m_buffer.pop_front();
    return tok;
}
