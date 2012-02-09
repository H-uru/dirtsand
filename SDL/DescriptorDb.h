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

#ifndef _SDL_DESCRIPTORDB_H
#define _SDL_DESCRIPTORDB_H

#include "StateInfo.h"
#include <vector>

namespace SDL
{
    struct VarDefault
    {
        bool m_valid;

        union
        {
            int32_t m_int;
            float m_float;
            double m_double;
            bool m_bool;

            DS::Vector3 m_vector;
            DS::Quaternion m_quat;
            DS::ColorRgba m_color;
            DS::ColorRgba8 m_color8;
        };
        DS::UnifiedTime m_time;
        DS::String m_string;

        VarDefault() : m_valid(false) { }
    };

    struct VarDescriptor
    {
        VarType m_type;
        DS::String m_typeName;
        DS::String m_name;
        int m_size;
        VarDefault m_default;
        DS::String m_defaultOption, m_displayOption;
    };

    struct StateDescriptor
    {
        DS::String m_name;
        int m_version;
        std::vector<VarDescriptor> m_vars;
        typedef std::unordered_map<DS::String, int, DS::StringHash> varmap_t;
        varmap_t m_varmap;
    };

    class DescriptorDb
    {
    public:
        static bool LoadDescriptors(const char* sdlpath);
        static StateDescriptor* FindDescriptor(DS::String name, int version);
        static StateDescriptor* FindLatestDescriptor(DS::String name);

    private:
        DescriptorDb() { }
        DescriptorDb(const DescriptorDb&) { }
        ~DescriptorDb() { }

        typedef std::unordered_map<int, StateDescriptor> versionmap_t;
        typedef std::unordered_map<DS::String, versionmap_t, DS::StringHash> descmap_t;
        static descmap_t s_descriptors;
    };
}

#endif
