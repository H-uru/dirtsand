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
#include <string_theory/format>
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
        ST::printf(stderr, "[SDL] Error reading SDL descriptors: {}\n", strerror(errno));
        return false;
    }
    if (count == 0) {
        fputs("[SDL] Warning: No SDL descriptors found!\n", stderr);
        free(dirls);
        return true;
    }

    SDL::Parser parser;
    for (int i=0; i<count; ++i) {
        ST::string filename = ST::format("{}/{}", sdlpath, dirls[i]->d_name);
        if (parser.open(filename.c_str())) {
            std::list<StateDescriptor> descriptors = parser.parse();
            for (auto it = descriptors.begin(); it != descriptors.end(); ++it) {
#ifdef DEBUG
                descmap_t::iterator namei = s_descriptors.find(it->m_name);
                if (namei != s_descriptors.end()) {
                    if (namei->second.find(it->m_version) != namei->second.end()) {
                        ST::printf(stderr, "[SDL] Warning: Duplicate descriptor version for {}\n",
                                   it->m_name);
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

SDL::StateDescriptor* SDL::DescriptorDb::FindDescriptor(const ST::string& name, int version)
{
    descmap_t::iterator namei = s_descriptors.find(name);
    if (namei == s_descriptors.end()) {
        ST::printf(stderr, "[SDL] Requested invalid descriptor {}\n", name);
        return nullptr;
    }

    versionmap_t::iterator veri = namei->second.find(version);
    if (veri == namei->second.end()) {
        ST::printf(stderr, "[SDL] Requested invalid descriptor version {} for {}\n",
                   version, name);
        return nullptr;
    }

    return &veri->second;
}

SDL::StateDescriptor* SDL::DescriptorDb::FindLatestDescriptor(const ST::string& name)
{
    descmap_t::iterator namei = s_descriptors.find(name);
    if (namei == s_descriptors.end()) {
        ST::printf(stderr, "[SDL] Requested invalid descriptor {}\n", name);
        return nullptr;
    }

    versionmap_t::iterator veri = namei->second.begin();
    versionmap_t::iterator newi = veri;
    for ( ; veri != namei->second.end(); ++veri) {
        if (veri->first > newi->first)
            newi = veri;
    }

    return &newi->second;
}

bool SDL::DescriptorDb::ForLatestDescriptors(descfunc_t functor)
{
    for (auto namei = s_descriptors.begin(); namei != s_descriptors.end(); ++namei) {
        auto veri = namei->second.begin();
        auto newi = veri;
        for ( ; veri != namei->second.end(); ++veri) {
            if (veri->first > newi->first)
                newi = veri;
        }
        if (!functor(namei->first, &newi->second))
            return false;
    }
    return true;
}

