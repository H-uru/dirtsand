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

#include "DescriptorDb.h"
#include "SdlParser.h"
#include "errors.h"
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static const char* toknames[] = {
    "e_TokStatedesc", "e_TokVersion", "e_TokVar", "e_TokInt", "e_TokFloat",
    "e_TokBool", "e_TokString", "e_TokPlKey", "e_TokCreatable", "e_TokDouble",
    "e_TokTime", "e_TokByte", "e_TokShort", "e_TokAgeTimeOfDay", "e_TokVector3",
    "e_TokPoint3", "e_TokQuat", "e_TokRgb", "e_TokRgb8", "e_TokRgba",
    "e_TokRgba8", "e_TokDefault", "e_TokDefaultOption",
    "e_TokIdent", "e_TokNumeric", "e_TokQuoted", "e_TokTypename",
};

static int sel_sdl(const dirent* de)
{
    return strcmp(strrchr(de->d_name, '.'), ".sdl") == 0;
}

bool SDL::DescriptorDb::LoadDescriptors(const char* sdlpath)
{
    dirent** dirls;
    int count = scandir(sdlpath, &dirls, &sel_sdl, &alphasort);
    if (count < 0) {
        fprintf(stderr, "[SDL] Error reading SDL descriptors: %s\n", strerror(errno));
        return false;
    }
    if (count == 0) {
        fprintf(stderr, "[SDL] Warning: No SDL descriptors found!\n");
        free(dirls);
        return true;
    }

    SDL::Parser parser;
    for (int i=0; i<count; ++i) {
        DS::String filename = DS::String::Format("%s/%s", sdlpath, dirls[i]->d_name);
        printf("%s:\n", filename.c_str());
        if (parser.open(filename.c_str())) {
            for ( ;; ) {
                SDL::Token tok = parser.next();
                if (tok.m_type == SDL::e_TokEof || tok.m_type == SDL::e_TokError)
                    break;
                if (tok.m_type < 256)
                    printf("  %ld: '%c'", tok.m_lineno, tok.m_type);
                else
                    printf("  %ld: %s", tok.m_lineno, toknames[tok.m_type - 256]);
                if (tok.m_type == SDL::e_TokIdent || tok.m_type == SDL::e_TokNumeric
                    || tok.m_type == SDL::e_TokQuoted || tok.m_type == SDL::e_TokTypename)
                    printf(" [%s]", tok.m_value.c_str());
                printf("\n");
            }
        }
        parser.close();
        free(dirls[i]);
    }
    free(dirls);
    return true;
}
