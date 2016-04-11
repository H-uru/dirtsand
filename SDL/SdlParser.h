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

#ifndef _SDL_PARSER_H
#define _SDL_PARSER_H

#include "DescriptorDb.h"
#include "strings.h"
#include <cstdio>
#include <list>

namespace SDL
{
    enum TokenType
    {
        e_TokEof = 0, e_TokError = -1,

        // Keywords
        e_TokStatedesc = 256, e_TokVersion, e_TokVar, e_TokInt, e_TokFloat,
        e_TokBool, e_TokString, e_TokPlKey, e_TokCreatable, e_TokDouble,
        e_TokTime, e_TokByte, e_TokShort, e_TokAgeTimeOfDay, e_TokVector3,
        e_TokPoint3, e_TokQuat, e_TokRgb, e_TokRgb8, e_TokRgba, e_TokRgba8,
        e_TokDefault, e_TokDefaultOption, e_TokDisplayOption,

        // Data-attached
        e_TokIdent, e_TokNumeric, e_TokQuoted, e_TokTypename,
    };

    struct Token
    {
        TokenType m_type;
        ST::string m_value;
        long m_lineno;
    };

    class Parser
    {
    public:
        Parser() : m_file(0) { }
        ~Parser() { close(); }

        bool open(const char* filename);
        void close()
        {
            if (m_file)
                fclose(m_file);
            m_file = 0;
            m_filename = ST::null;
        }

        const char* filename() const { return m_filename.c_str(); }

        Token next();
        void push(Token tok) { m_buffer.push_front(tok); }

        std::list<StateDescriptor> parse();

    private:
        FILE* m_file;
        ST::string m_filename;
        long m_lineno;
        std::list<Token> m_buffer;
    };
}

#endif
