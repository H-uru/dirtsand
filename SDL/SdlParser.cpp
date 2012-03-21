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

#include "SdlParser.h"

static const char* s_toknames[] = {
    "STATEDESC", "VERSION", "VAR", "INT", "FLOAT", "BOOL", "STRING", "PLKEY",
    "CREATABLE", "DOUBLE", "TIME", "BYTE", "SHORT", "AGETIMEOFDAY", "VECTOR3",
    "POINT3", "QUATERNION", "RGB", "RGB8", "RGBA", "RGBA8", "DEFAULT",
    "DEFAULTOPTION", "DISPLAYOPTION",
    "<Identifier>", "<Number>", "<String>", "<Typename>",
};

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
    m_buffer.clear();
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
    if (str == "DISPLAYOPTION")
        return SDL::e_TokDisplayOption;
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

enum {
    e_State_File,
    e_State_Statedesc_ID, e_State_Statedesc_BR, e_State_Statedesc,
    e_State_Version_N,
    e_State_Var_TP, e_State_Var_ID, e_State_Var_SZ, e_State_Var,
    e_State_Default_EQ, e_State_Default,
    e_State_DefaultOption_EQ, e_State_DefaultOption,
    e_State_DisplayOption_EQ, e_State_DisplayOption,
    e_State_SZ_N, e_State_SZ_X,
    e_State_LS_I, e_State_LS_X,
};

#define BAD_TOK(token, parser) \
    { \
        if ((token) < 256) \
            fprintf(stderr, "[SDL] %s:%ld: Unexpected token '%c'\n", \
                    parser->filename(), tok.m_lineno, (token)); \
        else \
            fprintf(stderr, "[SDL] %s:%ld: Unexpected token %s (%s)\n", \
                    parser->filename(), tok.m_lineno, s_toknames[(token)-256], \
                    tok.m_value.c_str()); \
        tok.m_type = SDL::e_TokError; \
        parser->push(tok); \
    }

#define CHECK_PARAMS(count) \
    if (values.size() != count) { \
        fprintf(stderr, "[SDL] %s:%ld: Incorrect number of parameters to DEFAULT attribute\n", \
                parser->filename(), tok.m_lineno); \
        tok.m_type = SDL::e_TokError; \
        parser->push(tok); \
    }

static int parse_default(SDL::VarDescriptor* var, SDL::Parser* parser)
{
    // Parse out any lists or parens
    int state = e_State_Default;
    std::vector<SDL::Token> values;
    SDL::Token tok;
    while (state != e_State_Var) {
        tok = parser->next();
        if (tok.m_type == SDL::e_TokError || tok.m_type == SDL::e_TokEof) {
            parser->push(tok);
            return state;
        }

        switch (tok.m_type) {
        case '(':
            if (state == e_State_Default) {
                state = e_State_LS_I;
            } else {
                BAD_TOK(tok.m_type, parser);
            }
            break;
        case ')':
            if (state == e_State_LS_X) {
                state = e_State_Var;
            } else {
                BAD_TOK(tok.m_type, parser);
            }
            break;
        case ',':
            if (state == e_State_LS_X) {
                state = e_State_LS_I;
            } else {
                BAD_TOK(tok.m_type, parser);
            }
            break;
        case SDL::e_TokNumeric:
            if (state == e_State_LS_I) {
                values.push_back(tok);
                state = e_State_LS_X;
            } else if (state == e_State_Default) {
                values.push_back(tok);
                state = e_State_Var;
            } else {
                BAD_TOK(tok.m_type, parser);
            }
            break;
        case SDL::e_TokIdent:
        case SDL::e_TokQuoted:
            if (state == e_State_Default) {
                values.push_back(tok);
                state = e_State_Var;
            } else {
                BAD_TOK(tok.m_type, parser);
            }
            break;
        default:
            BAD_TOK(tok.m_type, parser);
        }
    }

    switch (var->m_type) {
    case SDL::e_VarBool:
        CHECK_PARAMS(1);
        if (values[0].m_type == SDL::e_TokNumeric) {
            var->m_default.m_bool = (strtol(values[0].m_value.c_str(), 0, 0) != 0);
        } else if (values[0].m_type == SDL::e_TokIdent) {
            if (values[0].m_value == "true") {
                var->m_default.m_bool = true;
            } else if (values[0].m_value == "false") {
                var->m_default.m_bool = false;
            } else {
                fprintf(stderr, "[SDL] %s:%ld: Bad boolean value: %s\n",
                        parser->filename(), values[0].m_lineno,
                        values[0].m_value.c_str());
                tok.m_type = SDL::e_TokError;
                parser->push(tok);
            }
        } else {
            BAD_TOK(values[0].m_type, parser);
        }
        break;
    case SDL::e_VarInt:
    case SDL::e_VarByte:
    case SDL::e_VarShort:
        CHECK_PARAMS(1);
        if (values[0].m_type == SDL::e_TokNumeric) {
            var->m_default.m_int = strtol(values[0].m_value.c_str(), 0, 0);
        } else {
            BAD_TOK(values[0].m_type, parser);
        }
        break;
    case SDL::e_VarFloat:
        CHECK_PARAMS(1);
        if (values[0].m_type == SDL::e_TokNumeric) {
            var->m_default.m_float = strtof(values[0].m_value.c_str(), 0);
        } else {
            BAD_TOK(values[0].m_type, parser);
        }
        break;
    case SDL::e_VarDouble:
        CHECK_PARAMS(1);
        if (values[0].m_type == SDL::e_TokNumeric) {
            var->m_default.m_double = strtod(values[0].m_value.c_str(), 0);
        } else {
            BAD_TOK(values[0].m_type, parser);
        }
        break;
    case SDL::e_VarString:
        CHECK_PARAMS(1);
        if (values[0].m_type == SDL::e_TokIdent || values[0].m_type == SDL::e_TokQuoted) {
            var->m_default.m_string = values[0].m_value;
        } else {
            BAD_TOK(values[0].m_type, parser);
        }
        break;
    case SDL::e_VarTime:
        CHECK_PARAMS(1);
        if (values[0].m_type == SDL::e_TokNumeric) {
            var->m_default.m_time.m_secs = strtoul(values[0].m_value.c_str(), 0, 0);
            var->m_default.m_time.m_micros = 0;
        } else {
            BAD_TOK(values[0].m_type, parser);
        }
        break;
    case SDL::e_VarVector3:
    case SDL::e_VarPoint3:
        CHECK_PARAMS(3);
        if (values[0].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[0].m_type, parser);
        } else if (values[1].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[1].m_type, parser);
        } else if (values[2].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[2].m_type, parser);
        } else {
            var->m_default.m_vector.m_X = strtof(values[0].m_value.c_str(), 0);
            var->m_default.m_vector.m_Y = strtof(values[1].m_value.c_str(), 0);
            var->m_default.m_vector.m_Z = strtof(values[2].m_value.c_str(), 0);
        }
        break;
    case SDL::e_VarQuaternion:
        CHECK_PARAMS(4);
        if (values[0].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[0].m_type, parser);
        } else if (values[1].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[1].m_type, parser);
        } else if (values[2].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[2].m_type, parser);
        } else if (values[3].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[3].m_type, parser);
        } else {
            var->m_default.m_quat.m_X = strtof(values[0].m_value.c_str(), 0);
            var->m_default.m_quat.m_Y = strtof(values[1].m_value.c_str(), 0);
            var->m_default.m_quat.m_Z = strtof(values[2].m_value.c_str(), 0);
            var->m_default.m_quat.m_W = strtof(values[3].m_value.c_str(), 0);
        }
        break;
    case SDL::e_VarRgb:
        CHECK_PARAMS(3);
        if (values[0].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[0].m_type, parser);
        } else if (values[1].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[1].m_type, parser);
        } else if (values[2].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[2].m_type, parser);
        } else {
            var->m_default.m_color.m_R = strtof(values[0].m_value.c_str(), 0);
            var->m_default.m_color.m_G = strtof(values[1].m_value.c_str(), 0);
            var->m_default.m_color.m_B = strtof(values[2].m_value.c_str(), 0);
            var->m_default.m_color.m_A = 1.0f;
        }
        break;
    case SDL::e_VarRgba:
        CHECK_PARAMS(4);
        if (values[0].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[0].m_type, parser);
        } else if (values[1].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[1].m_type, parser);
        } else if (values[2].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[2].m_type, parser);
        } else if (values[3].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[3].m_type, parser);
        } else {
            var->m_default.m_color.m_R = strtof(values[0].m_value.c_str(), 0);
            var->m_default.m_color.m_G = strtof(values[1].m_value.c_str(), 0);
            var->m_default.m_color.m_B = strtof(values[2].m_value.c_str(), 0);
            var->m_default.m_color.m_A = strtof(values[3].m_value.c_str(), 0);
        }
        break;
    case SDL::e_VarRgb8:
        CHECK_PARAMS(3);
        if (values[0].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[0].m_type, parser);
        } else if (values[1].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[1].m_type, parser);
        } else if (values[2].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[2].m_type, parser);
        } else {
            var->m_default.m_color8.m_R = strtoul(values[0].m_value.c_str(), 0, 0);
            var->m_default.m_color8.m_G = strtoul(values[1].m_value.c_str(), 0, 0);
            var->m_default.m_color8.m_B = strtoul(values[2].m_value.c_str(), 0, 0);
            var->m_default.m_color8.m_A = 255;
        }
        break;
    case SDL::e_VarRgba8:
        CHECK_PARAMS(4);
        if (values[0].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[0].m_type, parser);
        } else if (values[1].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[1].m_type, parser);
        } else if (values[2].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[2].m_type, parser);
        } else if (values[3].m_type != SDL::e_TokNumeric) {
            BAD_TOK(values[3].m_type, parser);
        } else {
            var->m_default.m_color8.m_R = strtoul(values[0].m_value.c_str(), 0, 0);
            var->m_default.m_color8.m_G = strtoul(values[1].m_value.c_str(), 0, 0);
            var->m_default.m_color8.m_B = strtoul(values[2].m_value.c_str(), 0, 0);
            var->m_default.m_color8.m_A = strtoul(values[3].m_value.c_str(), 0, 0);
        }
        break;
    case SDL::e_VarKey:
        CHECK_PARAMS(1);
        if (values[0].m_type == SDL::e_TokIdent) {
            if (values[0].m_value == "nil") {
                // This is redundant...
            } else {
                fprintf(stderr, "[SDL] %s:%ld: Bad plKey initializer: %s\n",
                        parser->filename(), values[0].m_lineno,
                        values[0].m_value.c_str());
                tok.m_type = SDL::e_TokError;
                parser->push(tok);
            }
        } else {
            BAD_TOK(values[0].m_type, parser);
        }
        break;
    default:
        fprintf(stderr, "[SDL] %s:%ld: Unexpected variable type\n",
                parser->filename(), tok.m_lineno);
        break;
    }

    var->m_default.m_valid = true;
    return state;
}

#define TYPE_TOK(type) \
    if (state == e_State_Var_TP) { \
        varBuffer.m_type = type; \
        state = e_State_Var_ID; \
    } else { \
        BAD_TOK(tok.m_type, this); \
    }

std::list<SDL::StateDescriptor> SDL::Parser::parse()
{
    std::list<StateDescriptor> descriptors;

    StateDescriptor descBuffer;
    VarDescriptor varBuffer;
    int state = e_State_File;
    for ( ;; ) {
        SDL::Token tok = next();
        if (tok.m_type == e_TokError)
            break;
        if (tok.m_type == e_TokEof) {
            if (state != e_State_File)
                fprintf(stderr, "[SDL] Unexpected EOF in %s\n", m_filename.c_str());
            break;
        }

        switch (tok.m_type) {
        case '(':
            if (state == e_State_Default) {
                push(tok);
                state = parse_default(&varBuffer, this);
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case ')':
            BAD_TOK(tok.m_type, this);
            break;
        case ';':
            // Dunno what this is doing here...  >_>
            break;
        case '=':
            if (state == e_State_Default_EQ) {
                state = e_State_Default;
            } else if (state == e_State_DefaultOption_EQ) {
                state = e_State_DefaultOption;
            } else if (state == e_State_DisplayOption_EQ) {
                state = e_State_DisplayOption;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case '[':
            if (state == e_State_Var_SZ) {
                state = e_State_SZ_N;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case ']':
            if (state == e_State_SZ_N) {
                varBuffer.m_size = -1;
                state = e_State_Var;
            } else if (state == e_State_SZ_X) {
                state = e_State_Var;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case '{':
            if (state == e_State_Statedesc_BR) {
                state = e_State_Statedesc;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case '}':
            if (state == e_State_Var /*|| state == e_State_Var_SZ*/) {
                descBuffer.m_vars.push_back(varBuffer);
                descriptors.push_back(descBuffer);
                state = e_State_File;
            } else if (state == e_State_Statedesc) {
                for (size_t i = 0; i < descBuffer.m_vars.size(); ++i) {
                    descBuffer.m_varmap[descBuffer.m_vars[i].m_name] = i;
                }
                descriptors.push_back(descBuffer);
                state = e_State_File;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokStatedesc:
            if (state == e_State_File) {
                state = e_State_Statedesc_ID;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            descBuffer.m_name = DS::String();
            descBuffer.m_vars.clear();
            descBuffer.m_version = -1;
            break;
        case e_TokVersion:
            if (state == e_State_Var /*|| state == e_State_Var_SZ*/) {
                descBuffer.m_vars.push_back(varBuffer);
                state = e_State_Version_N;
            } else if (state == e_State_Statedesc) {
                state = e_State_Version_N;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokVar:
            if (state == e_State_Var /*|| state == e_State_Var_SZ*/) {
                descBuffer.m_vars.push_back(varBuffer);
                state = e_State_Var_TP;
            } else if (state == e_State_Statedesc) {
                state = e_State_Var_TP;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            varBuffer.m_type = static_cast<SDL::VarType>(-1);
            varBuffer.m_typeName = DS::String();
            varBuffer.m_name = DS::String();
            varBuffer.m_size = 0;
            varBuffer.m_default.m_valid = false;
            varBuffer.m_defaultOption = DS::String();
            break;
        case e_TokInt:
            TYPE_TOK(e_VarInt);
            break;
        case e_TokFloat:
            TYPE_TOK(e_VarFloat);
            break;
        case e_TokBool:
            TYPE_TOK(e_VarBool);
            break;
        case e_TokString:
            TYPE_TOK(e_VarString);
            break;
        case e_TokPlKey:
            TYPE_TOK(e_VarKey);
            break;
        case e_TokCreatable:
            TYPE_TOK(e_VarCreatable);
            break;
        case e_TokDouble:
            TYPE_TOK(e_VarDouble);
            break;
        case e_TokTime:
            TYPE_TOK(e_VarTime);
            break;
        case e_TokByte:
            TYPE_TOK(e_VarByte);
            break;
        case e_TokShort:
            TYPE_TOK(e_VarShort);
            break;
        case e_TokAgeTimeOfDay:
            TYPE_TOK(e_VarAgeTimeOfDay);
            break;
        case e_TokVector3:
            TYPE_TOK(e_VarVector3);
            break;
        case e_TokPoint3:
            TYPE_TOK(e_VarPoint3);
            break;
        case e_TokQuat:
            TYPE_TOK(e_VarQuaternion);
            break;
        case e_TokRgb:
            TYPE_TOK(e_VarRgb);
            break;
        case e_TokRgb8:
            TYPE_TOK(e_VarRgb8);
            break;
        case e_TokRgba:
            TYPE_TOK(e_VarRgba);
            break;
        case e_TokRgba8:
            TYPE_TOK(e_VarRgba8);
            break;
        case e_TokDefault:
            if (/*state == e_State_Var_SZ ||*/ state == e_State_Var) {
                state = e_State_Default_EQ;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokDefaultOption:
            if (/*state == e_State_Var_SZ ||*/ state == e_State_Var) {
                state = e_State_DefaultOption_EQ;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokDisplayOption:
            if (/*state == e_State_Var_SZ ||*/ state == e_State_Var) {
                state = e_State_DisplayOption_EQ;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokIdent:
            if (state == e_State_Statedesc_ID) {
                descBuffer.m_name = tok.m_value;
                state = e_State_Statedesc_BR;
            } else if (state == e_State_Var_ID) {
                varBuffer.m_name = tok.m_value;
                state = e_State_Var_SZ;
            } else if (state == e_State_Default) {
                push(tok);
                state = parse_default(&varBuffer, this);
            } else if (state == e_State_DefaultOption) {
                varBuffer.m_defaultOption = tok.m_value;
                state = e_State_Var;
            } else if (state == e_State_DisplayOption) {
                varBuffer.m_displayOption = tok.m_value;
                state = e_State_Var;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokNumeric:
            if (state == e_State_Version_N) {
                descBuffer.m_version = strtol(tok.m_value.c_str(), 0, 0);
                state = e_State_Statedesc;
            } else if (state == e_State_SZ_N) {
                varBuffer.m_size = strtol(tok.m_value.c_str(), 0, 0);
                state = e_State_SZ_X;
            } else if (state == e_State_Default) {
                push(tok);
                state = parse_default(&varBuffer, this);
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokQuoted:
            if (state == e_State_Default) {
                push(tok);
                state = parse_default(&varBuffer, this);
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        case e_TokTypename:
            if (state == e_State_Var_TP) {
                varBuffer.m_type = e_VarStateDesc;
                varBuffer.m_typeName = tok.m_value;
                state = e_State_Var_ID;
            } else {
                BAD_TOK(tok.m_type, this);
            }
            break;
        default:
            BAD_TOK(tok.m_type, this);
            break;
        }
    }

    return descriptors;
}
