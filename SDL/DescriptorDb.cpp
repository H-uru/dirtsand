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
        if (parser.open(filename.c_str())) {
            parser.parse();
        }
        parser.close();
        free(dirls[i]);
    }
    free(dirls);
    return true;
}
