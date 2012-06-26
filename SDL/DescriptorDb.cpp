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

SDL::DescriptorDb::descmap_t SDL::DescriptorDb::s_descriptors;

bool SDL::DescriptorDb::LoadDescriptors(const char* sdlpath)
{
    dirent** dirls;
    int count = scandir(sdlpath, &dirls, &sel_sdl, &alphasort);
    if (count < 0) {
        fprintf(stderr, "[SDL] Error reading SDL descriptors: %s\n", strerror(errno));
        return false;
    }
    if (count == 0) {
        fputs("[SDL] Warning: No SDL descriptors found!\n", stderr);
        free(dirls);
        return true;
    }

    SDL::Parser parser;
    for (int i=0; i<count; ++i) {
        DS::String filename = DS::String::Format("%s/%s", sdlpath, dirls[i]->d_name);
        if (parser.open(filename.c_str())) {
            std::list<StateDescriptor> descriptors = parser.parse();
            for (auto it = descriptors.begin(); it != descriptors.end(); ++it) {
#ifdef DEBUG
                descmap_t::iterator namei = s_descriptors.find(it->m_name);
                if (namei != s_descriptors.end()) {
                    if (namei->second.find(it->m_version) != namei->second.end()) {
                        fprintf(stderr, "[SDL] Warning: Duplicate descriptor version for %s\n",
                                it->m_name.c_str());
                    }
                }
#endif
                s_descriptors[it->m_name][it->m_version] = *it;

                // Keep the highest version in -1
                if (s_descriptors[it->m_name][-1].m_version < it->m_version)
                    s_descriptors[it->m_name][-1] = *it;
            }
        }
        parser.close();
        free(dirls[i]);
    }
    free(dirls);
    return true;
}

SDL::StateDescriptor* SDL::DescriptorDb::FindDescriptor(DS::String name, int version)
{
    descmap_t::iterator namei = s_descriptors.find(name);
    if (namei == s_descriptors.end()) {
        fprintf(stderr, "[SDL] Requested invalid descriptor %s\n", name.c_str());
        return 0;
    }

    versionmap_t::iterator veri = namei->second.find(version);
    if (veri == namei->second.end()) {
        fprintf(stderr, "[SDL] Requested invalid decriptor version %d for %s\n",
                version, name.c_str());
        return 0;
    }

    return &veri->second;
}

SDL::StateDescriptor* SDL::DescriptorDb::FindLatestDescriptor(DS::String name)
{
    descmap_t::iterator namei = s_descriptors.find(name);
    if (namei == s_descriptors.end()) {
        fprintf(stderr, "[SDL] Requested invalid descriptor %s\n", name.c_str());
        return 0;
    }

    versionmap_t::iterator veri = namei->second.begin();
    versionmap_t::iterator newi = veri;
    for ( ; veri != namei->second.end(); ++veri) {
        if (veri->first > newi->first)
            newi = veri;
    }

    return &newi->second;
}
