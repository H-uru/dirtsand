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
        e_VarRgb8, e_VarRgba8, e_VarStateDesc, e_VarInvalid = -1
    };

    struct Value
    {
        VarType m_type;

        union   // "Basic" types
        {
            int32_t m_int;
            int16_t m_short;
            int8_t m_byte;
            float m_float;
            double m_double;
            bool m_bool;

            DS::Vector3 m_vector;
            DS::Quaternion m_quat;
            DS::ColorRgba m_color;
            DS::ColorRgba8 m_color8;

            MOUL::Creatable* m_creatable;
        };
        DS::UnifiedTime m_time;
        DS::String m_string;
        MOUL::Key m_key;

        Value() : m_type(e_VarInvalid) { }
        Value(const Value& other);

        ~Value() { clear(); }
        void clear();

        Value& operator=(const Value& other);
    };
}

#endif
