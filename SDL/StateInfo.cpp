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

#include "StateInfo.h"

SDL::Value::Value(const SDL::Value& other)
    : m_type(other.m_type)
{
    switch (m_type) {
    case e_VarInt:
        m_int = other.m_int;
        break;
    case e_VarFloat:
        m_float = other.m_float;
        break;
    case e_VarBool:
        m_bool = other.m_bool;
        break;
    case e_VarString:
        m_string = other.m_string;
        break;
    case e_VarKey:
        m_key = other.m_key;
        break;
    case e_VarCreatable:
        m_creatable = other.m_creatable;
        m_creatable->ref();
        break;
    case e_VarDouble:
        m_double = other.m_double;
        break;
    case e_VarTime:
        m_time = other.m_time;
        break;
    case e_VarByte:
        m_byte = other.m_byte;
        break;
    case e_VarShort:
        m_short = other.m_short;
        break;
    case e_VarVector3:
    case e_VarPoint3:
        m_vector = other.m_vector;
        break;
    case e_VarQuaternion:
        m_quat = other.m_quat;
        break;
    case e_VarRgb:
    case e_VarRgba:
        m_color = other.m_color;
        break;
    case e_VarRgb8:
    case e_VarRgba8:
        m_color8 = other.m_color8;
        break;
    case e_VarStateDesc:
        //TODO
        break;
    default:
        break;
    }
}

void SDL::Value::clear()
{
    if (m_type == e_VarCreatable)
        m_creatable->unref();
    m_type = e_VarInvalid;
}

SDL::Value& SDL::Value::operator=(const SDL::Value& other)
{
    if (other.m_type == e_VarCreatable)
        other.m_creatable->ref();
    if (m_type == e_VarCreatable)
        m_creatable->unref();

    switch (other.m_type) {
    case e_VarInt:
        m_int = other.m_int;
        break;
    case e_VarFloat:
        m_float = other.m_float;
        break;
    case e_VarBool:
        m_bool = other.m_bool;
        break;
    case e_VarString:
        m_string = other.m_string;
        break;
    case e_VarKey:
        m_key = other.m_key;
        break;
    case e_VarCreatable:
        m_creatable = other.m_creatable;
        break;
    case e_VarDouble:
        m_double = other.m_double;
        break;
    case e_VarTime:
        m_time = other.m_time;
        break;
    case e_VarByte:
        m_byte = other.m_byte;
        break;
    case e_VarShort:
        m_short = other.m_short;
        break;
    case e_VarVector3:
    case e_VarPoint3:
        m_vector = other.m_vector;
        break;
    case e_VarQuaternion:
        m_quat = other.m_quat;
        break;
    case e_VarRgb:
    case e_VarRgba:
        m_color = other.m_color;
        break;
    case e_VarRgb8:
    case e_VarRgba8:
        m_color8 = other.m_color8;
        break;
    case e_VarStateDesc:
        //TODO
        break;
    default:
        break;
    }

    m_type = other.m_type;
    return *this;
}
