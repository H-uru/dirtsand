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

#ifndef _SDL_DESCRIPTORDB_H
#define _SDL_DESCRIPTORDB_H

#include "StateInfo.h"
#include <functional>
#include <vector>
#include <unordered_map>

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
        ST::string m_string;

        VarDefault() : m_valid(false) { }
    };

    struct VarDescriptor
    {
        VarType m_type;
        ST::string m_typeName;
        ST::string m_name;
        int m_size;
        VarDefault m_default;
        ST::string m_defaultOption, m_displayOption;
    };

    struct StateDescriptor
    {
        ST::string m_name;
        int m_version;
        std::vector<VarDescriptor> m_vars;
        typedef std::unordered_map<ST::string, int, ST::hash> varmap_t;
        varmap_t m_varmap;
    };

    class DescriptorDb
    {
    public:
        typedef std::function<bool(const ST::string&, StateDescriptor*)> descfunc_t;

        static bool LoadDescriptors(const char* sdlpath);
        static StateDescriptor* FindDescriptor(const ST::string& name, int version);
        static StateDescriptor* FindLatestDescriptor(const ST::string& name);
        static bool ForLatestDescriptors(descfunc_t functor);

    private:
        DescriptorDb() = delete;
        DescriptorDb(const DescriptorDb&) = delete;
        ~DescriptorDb() = delete;

        typedef std::unordered_map<int, StateDescriptor> versionmap_t;
        typedef std::unordered_map<ST::string, versionmap_t, ST::hash_i, ST::equal_i> descmap_t;
        static descmap_t s_descriptors;
    };
}

#endif
