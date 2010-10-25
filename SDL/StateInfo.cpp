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
#include "DescriptorDb.h"
#include "errors.h"

SDL::Variable::_ref::_ref(SDL::VarDescriptor* desc)
    : m_refs(1), m_desc(desc)
{
    switch (m_desc->m_type) {
    case e_VarInt:
        m_int = new int32_t[m_desc->m_size];
        memset(m_int, 0, m_desc->m_size * sizeof(int32_t));
        break;
    case e_VarFloat:
        m_float = new float[m_desc->m_size];
        memset(m_float, 0, m_desc->m_size * sizeof(float));
        break;
    case e_VarBool:
        m_bool = new bool[m_desc->m_size];
        memset(m_bool, 0, m_desc->m_size * sizeof(bool));
        break;
    case e_VarString:
        m_string = new DS::String[m_desc->m_size];
        break;
    case e_VarKey:
        m_key = new MOUL::Key[m_desc->m_size];
        break;
    case e_VarCreatable:
        m_creatable = new MOUL::Creatable*[m_desc->m_size];
        memset(m_creatable, 0, m_desc->m_size * sizeof(MOUL::Creatable*));
        break;
    case e_VarDouble:
        m_double = new double[m_desc->m_size];
        memset(m_double, 0, m_desc->m_size * sizeof(double));
        break;
    case e_VarTime:
        m_time = new DS::UnifiedTime[m_desc->m_size];
        break;
    case e_VarByte:
        m_byte = new int8_t[m_desc->m_size];
        memset(m_byte, 0, m_desc->m_size * sizeof(int8_t));
        break;
    case e_VarShort:
        m_short = new int16_t[m_desc->m_size];
        memset(m_short, 0, m_desc->m_size * sizeof(int16_t));
        break;
    case e_VarVector3:
    case e_VarPoint3:
        m_vector = new DS::Vector3[m_desc->m_size];
        memset(m_vector, 0, m_desc->m_size * sizeof(DS::Vector3));
        break;
    case e_VarQuaternion:
        m_quat = new DS::Quaternion[m_desc->m_size];
        memset(m_quat, 0, m_desc->m_size * sizeof(DS::Quaternion));
        break;
    case e_VarRgb:
    case e_VarRgba:
        m_color = new DS::ColorRgba[m_desc->m_size];
        memset(m_color, 0, m_desc->m_size * sizeof(DS::ColorRgba));
        break;
    case e_VarRgb8:
    case e_VarRgba8:
        m_color8 = new DS::ColorRgba8[m_desc->m_size];
        memset(m_color8, 0, m_desc->m_size * sizeof(DS::ColorRgba8));
        break;
    case e_VarStateDesc:
        m_child = new SDL::State[m_desc->m_size];
        break;
    default:
        break;
    }
}

SDL::Variable::_ref::~_ref()
{
    switch (m_desc->m_type) {
    case e_VarInt:
        delete[] m_int;
        break;
    case e_VarFloat:
        delete[] m_float;
        break;
    case e_VarBool:
        delete[] m_bool;
        break;
    case e_VarString:
        delete[] m_string;
        break;
    case e_VarKey:
        delete[] m_key;
        break;
    case e_VarCreatable:
        for (int i=0; i<m_desc->m_size; ++i) {
            if (m_creatable[i])
                m_creatable[i]->unref();
        }
        delete[] m_creatable;
        break;
    case e_VarDouble:
        delete[] m_double;
        break;
    case e_VarTime:
        delete[] m_time;
        break;
    case e_VarByte:
        delete[] m_byte;
        break;
    case e_VarShort:
        delete[] m_short;
        break;
    case e_VarVector3:
    case e_VarPoint3:
        delete[] m_vector;
        break;
    case e_VarQuaternion:
        delete[] m_quat;
        break;
    case e_VarRgb:
    case e_VarRgba:
        delete[] m_color;
        break;
    case e_VarRgb8:
    case e_VarRgba8:
        delete[] m_color8;
        break;
    case e_VarStateDesc:
        delete[] m_child;
        break;
    default:
        break;
    }
}
