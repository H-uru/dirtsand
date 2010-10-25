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

#ifndef _SDL_STATEINFO_H
#define _SDL_STATEINFO_H

#include "PlasMOUL/Key.h"
#include "PlasMOUL/creatable.h"
#include "Types/UnifiedTime.h"
#include "Types/Math.h"
#include "Types/Color.h"
#include "strings.h"

namespace SDL
{
    enum VarType
    {
        e_VarInt, e_VarFloat, e_VarBool, e_VarString, e_VarKey, e_VarCreatable,
        e_VarDouble, e_VarTime, e_VarByte, e_VarShort, e_VarAgeTimeOfDay,
        e_VarVector3, e_VarPoint3, e_VarQuaternion, e_VarRgb, e_VarRgba,
        e_VarRgb8, e_VarRgba8, e_VarStateDesc,
    };

    struct StateDescriptor;
    struct VarDescriptor;

    class Variable
    {
    public:
        Variable(VarDescriptor* desc = 0) : m_data(0)
        {
            if (desc)
                m_data = new _ref(desc);
        }

        Variable(const Variable& copy)
            : m_data(copy.m_data)
        {
            if (m_data)
                m_data->ref();
        }

        ~Variable()
        {
            if (m_data)
                m_data->unref();
        }

    private:
        struct _ref
        {
            union
            {
                int32_t* m_int;
                int16_t* m_short;
                int8_t* m_byte;
                float* m_float;
                double* m_double;
                bool* m_bool;

                DS::String* m_string;
                DS::UnifiedTime* m_time;
                DS::Vector3* m_vector;
                DS::Quaternion* m_quat;
                DS::ColorRgba* m_color;
                DS::ColorRgba8* m_color8;

                MOUL::Key* m_key;
                MOUL::Creatable** m_creatable;
                class State* m_child;
            };
            int m_refs;
            VarDescriptor* m_desc;

            _ref(VarDescriptor* desc);
            ~_ref();

            void ref() { ++m_refs; }
            void unref()
            {
                if (--m_refs == 0)
                    delete this;
            }
        }* m_data;

    public:
        VarDescriptor* descriptor() const { return m_data ? m_data->m_desc : 0; }
        _ref* data() { return m_data; }
    };

    class State
    {
    public:
        StateDescriptor* m_desc;
        std::vector<Variable> m_vars;

        State(StateDescriptor* desc = 0) : m_desc(desc) { }
    };
}

#endif
