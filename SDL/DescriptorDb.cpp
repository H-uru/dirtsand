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

bool SDL::DescriptorDb::LoadDescriptorsFromFile(const ST::string& filename)
{
    SDL::Parser parser;
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
    return true;
}

bool SDL::DescriptorDb::LoadDescriptors(const char* sdlpath)
{
    try {
        ForDescriptorFiles(sdlpath, LoadDescriptorsFromFile);
    } catch (const DS::SystemError& err) {
        fputs(err.what(), stderr);
        return false;
    }
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

bool SDL::DescriptorDb::ForDescriptorFiles(const char* sdlpath, filefunc_t functor)
{
    dirent** dirls;
    int count = scandir(sdlpath, &dirls, &sel_sdl, &alphasort);

    DS_ASSERT(count > 0);
    if (count == 0)
        fputs("[SDL] Warning: No SDL descriptors found!\n", stderr);
    if (count < 0)
        throw DS::SystemError("[SDL] Error scanning for SDL files", strerror(errno));

    bool retval = true;
    for (int i = 0; i < count; i++) {
        if (!functor(ST::format("{}/{}", sdlpath, dirls[i]->d_name))) {
            retval = false;
            break;
        }
    }

    for (int i = 0; i < count; i++)
        free(dirls[i]);
    free(dirls);
    return retval;
}
