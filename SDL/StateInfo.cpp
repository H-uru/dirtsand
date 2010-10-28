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
#include "PlasMOUL/factory.h"
#include "Types/BitVector.h"
#include "errors.h"

static size_t stupidLengthRead(DS::Stream* stream, size_t max)
{
    if (max < 0x100)
        return stream->read<uint8_t>();
    if (max < 0x10000)
        return stream->read<uint16_t>();
    return stream->read<uint32_t>();
}

static void stupidLengthWrite(DS::Stream* stream, size_t max, size_t value)
{
    if (max < 0x100)
        stream->write<uint8_t>(value);
    else if (max < 0x10000)
        stream->write<uint16_t>(value);
    else
        stream->write<uint32_t>(value);
}

SDL::Variable::_ref::_ref(SDL::VarDescriptor* desc)
    : m_size(0), m_flags(0), m_refs(1), m_desc(desc)
{
    if (m_desc->m_size > 0)
        resize(m_desc->m_size);
}

void SDL::Variable::_ref::resize(size_t size)
{
    clear();
    m_size = size;
    if (m_size == 0)
        return;

    switch (m_desc->m_type) {
    case e_VarInt:
        m_int = new int32_t[m_size];
        memset(m_int, 0, m_size * sizeof(int32_t));
        break;
    case e_VarFloat:
        m_float = new float[m_size];
        memset(m_float, 0, m_size * sizeof(float));
        break;
    case e_VarBool:
        m_bool = new bool[m_size];
        memset(m_bool, 0, m_size * sizeof(bool));
        break;
    case e_VarString:
        m_string = new DS::String[m_size];
        break;
    case e_VarKey:
        m_key = new MOUL::Uoid[m_size];
        break;
    case e_VarCreatable:
        m_creatable = new MOUL::Creatable*[m_size];
        memset(m_creatable, 0, m_size * sizeof(MOUL::Creatable*));
        break;
    case e_VarDouble:
        m_double = new double[m_size];
        memset(m_double, 0, m_size * sizeof(double));
        break;
    case e_VarTime:
        m_time = new DS::UnifiedTime[m_size];
        break;
    case e_VarByte:
        m_byte = new int8_t[m_size];
        memset(m_byte, 0, m_size * sizeof(int8_t));
        break;
    case e_VarShort:
        m_short = new int16_t[m_size];
        memset(m_short, 0, m_size * sizeof(int16_t));
        break;
    case e_VarVector3:
    case e_VarPoint3:
        m_vector = new DS::Vector3[m_size];
        memset(m_vector, 0, m_size * sizeof(DS::Vector3));
        break;
    case e_VarQuaternion:
        m_quat = new DS::Quaternion[m_size];
        memset(m_quat, 0, m_size * sizeof(DS::Quaternion));
        break;
    case e_VarRgb:
    case e_VarRgba:
        m_color = new DS::ColorRgba[m_size];
        memset(m_color, 0, m_size * sizeof(DS::ColorRgba));
        break;
    case e_VarRgb8:
    case e_VarRgba8:
        m_color8 = new DS::ColorRgba8[m_size];
        memset(m_color8, 0, m_size * sizeof(DS::ColorRgba8));
        break;
    case e_VarStateDesc:
        m_child = new SDL::State[m_size];
        break;
    default:
        break;
    }
}

void SDL::Variable::_ref::clear()
{
    m_flags &= ~e_XIsDirty;
    if (m_size == 0)
        return;

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
        for (int i=0; i<m_desc->m_size; ++i)
            m_creatable[i]->unref();
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

void SDL::Variable::_ref::read(DS::Stream* stream)
{
    m_flags |= e_XIsDirty;

    for (size_t i=0; i<m_size; ++i) {
        switch (m_desc->m_type) {
        case e_VarInt:
            m_int[i] = stream->read<int32_t>();
            break;
        case e_VarFloat:
            m_float[i] = stream->read<float>();
            break;
        case e_VarBool:
            m_bool[i] = stream->readBool();
            break;
        case e_VarString:
            {
                char buffer[33];
                stream->readBytes(buffer, 32);
                buffer[32] = 0;
                m_string[i] = buffer;
            }
            break;
        case e_VarKey:
            m_key[i].read(stream);
            break;
        case e_VarCreatable:
            {
                uint16_t type = stream->read<uint16_t>();
                if (type != 0x8000) {
                    m_creatable[i] = MOUL::Factory::Create(type);
                    DS_DASSERT(m_creatable[i] != 0);
                    uint32_t endp = stream->tell() + stream->read<uint32_t>();
                    m_creatable[i]->read(stream);
                    DS_DASSERT(stream->tell() == endp);
                }
            }
            break;
        case e_VarDouble:
            m_double[i] = stream->read<double>();
            break;
        case e_VarTime:
            m_time[i].read(stream);
            break;
        case e_VarByte:
            m_byte[i] = stream->read<uint8_t>();
            break;
        case e_VarShort:
            m_short[i] = stream->read<uint16_t>();
            break;
        case e_VarVector3:
        case e_VarPoint3:
            m_vector[i] = stream->read<DS::Vector3>();
            break;
        case e_VarQuaternion:
            m_quat[i] = stream->read<DS::Quaternion>();
            break;
        case e_VarRgb:
            m_color[i].m_R = stream->read<float>();
            m_color[i].m_G = stream->read<float>();
            m_color[i].m_B = stream->read<float>();
            break;
        case e_VarRgba:
            m_color[i].m_R = stream->read<float>();
            m_color[i].m_G = stream->read<float>();
            m_color[i].m_B = stream->read<float>();
            m_color[i].m_A = stream->read<float>();
            break;
        case e_VarRgb8:
            m_color8[i].m_R = stream->read<uint8_t>();
            m_color8[i].m_G = stream->read<uint8_t>();
            m_color8[i].m_B = stream->read<uint8_t>();
            break;
        case e_VarRgba8:
            m_color8[i].m_R = stream->read<uint8_t>();
            m_color8[i].m_G = stream->read<uint8_t>();
            m_color8[i].m_B = stream->read<uint8_t>();
            m_color8[i].m_A = stream->read<uint8_t>();
            break;
        case e_VarStateDesc:
            DS_DASSERT(0);
            break;
        default:
            break;
        }
    }
}

void SDL::Variable::_ref::write(DS::Stream* stream)
{
    for (size_t i=0; i<m_size; ++i) {
        switch (m_desc->m_type) {
        case e_VarInt:
            stream->write<int32_t>(m_int[i]);
            break;
        case e_VarFloat:
            stream->write<float>(m_float[i]);
            break;
        case e_VarBool:
            stream->writeBool(m_bool[i]);
            break;
        case e_VarString:
            {
                char buffer[32];
                memset(buffer, 0, 32);
                strncpy(buffer, m_string[i].c_str(), 32);
                buffer[31] = 0;
                stream->writeBytes(buffer, 32);
            }
            break;
        case e_VarKey:
            m_key[i].write(stream);
            break;
        case e_VarCreatable:
            if (!m_creatable[i]) {
                stream->write<uint16_t>(0x8000);
            } else {
                stream->write<uint16_t>(m_creatable[i]->type());
                uint32_t sizepos = stream->tell();
                stream->write<uint32_t>(0);
                m_creatable[i]->write(stream);
                uint32_t endpos = stream->tell();
                stream->seek(sizepos, SEEK_SET);
                stream->write<uint32_t>(endpos - sizepos - sizeof(uint32_t));
                stream->seek(endpos, SEEK_SET);
            }
            break;
        case e_VarDouble:
            stream->write<double>(m_double[i]);
            break;
        case e_VarTime:
            m_time[i].write(stream);
            break;
        case e_VarByte:
            stream->write<uint8_t>(m_byte[i]);
            break;
        case e_VarShort:
            stream->write<uint16_t>(m_short[i]);
            break;
        case e_VarVector3:
        case e_VarPoint3:
            stream->write<DS::Vector3>(m_vector[i]);
            break;
        case e_VarQuaternion:
            stream->write<DS::Quaternion>(m_quat[i]);
            break;
        case e_VarRgb:
            stream->write<float>(m_color[i].m_R);
            stream->write<float>(m_color[i].m_G);
            stream->write<float>(m_color[i].m_B);
            break;
        case e_VarRgba:
            stream->write<float>(m_color[i].m_R);
            stream->write<float>(m_color[i].m_G);
            stream->write<float>(m_color[i].m_B);
            stream->write<float>(m_color[i].m_A);
            break;
        case e_VarRgb8:
            stream->write<uint8_t>(m_color8[i].m_R);
            stream->write<uint8_t>(m_color8[i].m_G);
            stream->write<uint8_t>(m_color8[i].m_B);
            break;
        case e_VarRgba8:
            stream->write<uint8_t>(m_color8[i].m_R);
            stream->write<uint8_t>(m_color8[i].m_G);
            stream->write<uint8_t>(m_color8[i].m_B);
            stream->write<uint8_t>(m_color8[i].m_A);
            break;
        case e_VarStateDesc:
            DS_DASSERT(0);
            break;
        default:
            break;
        }
    }
}

void SDL::Variable::read(DS::Stream* stream)
{
    uint8_t contents = stream->read<uint8_t>();
    if (contents & e_HasNotificationInfo) {
        stream->read<uint8_t>();    // Ignored
        m_data->m_notificationHint = stream->readSafeString();
    }

    m_data->m_flags = (m_data->m_flags & ~0xFF) | stream->read<uint8_t>();
    if (m_data->m_desc->m_type == e_VarStateDesc) {
        if (m_data->m_desc->m_size == -1) {
            size_t count = stream->read<uint32_t>();
            DS_DASSERT(count < 10000);
            m_data->resize(count);
        }
        size_t count = stupidLengthRead(stream, m_data->m_size);
        bool useIndices = (count != m_data->m_size);
        for (size_t i=0; i<count; ++i) {
            size_t idx = useIndices ? stupidLengthRead(stream, m_data->m_size) : i;
            m_data->m_child[idx].read(stream);
        }
    } else {
        if (m_data->m_flags & e_HasTimeStamp)
            m_data->m_timestamp.read(stream);

        if (!(m_data->m_flags & e_SameAsDefault)) {
            if (m_data->m_desc->m_size == -1) {
                size_t count = stream->read<uint32_t>();
                DS_DASSERT(count < 10000);
                m_data->resize(count);
            }
            m_data->read(stream);
        }
    }
}

void SDL::Variable::write(DS::Stream* stream)
{
    uint8_t contents = 0;
    if (!m_data->m_notificationHint.isNull())
        contents |= e_HasNotificationInfo;
    stream->write<uint8_t>(contents);
    if (contents & e_HasNotificationInfo) {
        stream->write<uint8_t>(0);
        stream->writeSafeString(m_data->m_notificationHint);
    }

    stream->write<uint8_t>(m_data->m_flags & 0xFF);
    if (m_data->m_desc->m_type == e_VarStateDesc) {
        if (m_data->m_desc->m_size == -1)
            stream->write<uint32_t>(m_data->m_size);

        DS::BitVector dirty;
        size_t count = 0;
        for (size_t i=0; i<m_data->m_size; ++i) {
            dirty.set(i, m_data->m_child[i].isDirty());
            ++count;
        }
        stupidLengthWrite(stream, m_data->m_size, count);
        bool useIndices = (count != m_data->m_size);
        for (size_t i=0; i<m_data->m_size; ++i) {
            if (dirty.get(i)) {
                if (useIndices)
                    stupidLengthWrite(stream, m_data->m_size, i);
                m_data->m_child[i].write(stream);
            }
        }
    } else {
        if (m_data->m_flags & e_HasTimeStamp)
            m_data->m_timestamp.write(stream);

        if (isDefault())
            m_data->m_flags |= e_SameAsDefault;
        if (!(m_data->m_flags & e_SameAsDefault)) {
            if (m_data->m_desc->m_size == -1)
                stream->write<uint32_t>(m_data->m_size);
            m_data->write(stream);
        }
    }
}

void SDL::Variable::setDefault()
{
    DS_DASSERT(m_data != 0);

    for (size_t i=0; i<m_data->m_size; ++i) {
        switch (m_data->m_desc->m_type) {
        case e_VarInt:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_int[i] = m_data->m_desc->m_default.m_int;
            else
                m_data->m_int[i] = 0;
            break;
        case e_VarFloat:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_float[i] = m_data->m_desc->m_default.m_float;
            else
                m_data->m_float[i] = 0;
            break;
        case e_VarBool:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_bool[i] = m_data->m_desc->m_default.m_bool;
            else
                m_data->m_bool[i] = false;
            break;
        case e_VarString:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_string[i] = m_data->m_desc->m_default.m_string;
            else
                m_data->m_string[i] = DS::String();
            break;
        case e_VarKey:
            m_data->m_key[i] = MOUL::Uoid();
            break;
        case e_VarCreatable:
            m_data->m_creatable[i]->unref();
            m_data->m_creatable[i] = 0;
            break;
        case e_VarDouble:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_double[i] = m_data->m_desc->m_default.m_double;
            else
                m_data->m_double[i] = 0;
            break;
        case e_VarTime:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_time[i] = m_data->m_desc->m_default.m_time;
            else
                m_data->m_time[i] = DS::UnifiedTime();
            break;
        case e_VarByte:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_byte[i] = m_data->m_desc->m_default.m_int;
            else
                m_data->m_byte[i] = 0;
            break;
        case e_VarShort:
            if (m_data->m_desc->m_default.m_valid)
                m_data->m_short[i] = m_data->m_desc->m_default.m_int;
            else
                m_data->m_short[i] = 0;
            break;
        case e_VarVector3:
        case e_VarPoint3:
            if (m_data->m_desc->m_default.m_valid) {
                m_data->m_vector[i] = m_data->m_desc->m_default.m_vector;
            } else {
                m_data->m_vector[i].m_X = 0;
                m_data->m_vector[i].m_Y = 0;
                m_data->m_vector[i].m_Z = 0;
            }
            break;
        case e_VarQuaternion:
            if (m_data->m_desc->m_default.m_valid) {
                m_data->m_quat[i] = m_data->m_desc->m_default.m_quat;
            } else {
                m_data->m_quat[i].m_X = 0;
                m_data->m_quat[i].m_Y = 0;
                m_data->m_quat[i].m_Z = 0;
                m_data->m_quat[i].m_W = 1;
            }
            break;
        case e_VarRgb:
        case e_VarRgba:
            if (m_data->m_desc->m_default.m_valid) {
                m_data->m_color[i] = m_data->m_desc->m_default.m_color;
            } else {
                m_data->m_color[i].m_R = 0;
                m_data->m_color[i].m_G = 0;
                m_data->m_color[i].m_B = 0;
                m_data->m_color[i].m_A = 1;
            }
            break;
        case e_VarRgb8:
        case e_VarRgba8:
            if (m_data->m_desc->m_default.m_valid) {
                m_data->m_color8[i] = m_data->m_desc->m_default.m_color8;
            } else {
                m_data->m_color8[i].m_R = 0;
                m_data->m_color8[i].m_G = 0;
                m_data->m_color8[i].m_B = 0;
                m_data->m_color8[i].m_A = 255;
            }
            break;
        case e_VarStateDesc:
            m_data->m_child[i].setDefault();
            break;
        default:
            break;
        }
    }

    m_data->m_flags |= e_SameAsDefault;
}

bool SDL::Variable::isDefault() const
{
    DS_DASSERT(m_data != 0);

    for (size_t i=0; i<m_data->m_size; ++i) {
        switch (m_data->m_desc->m_type) {
        case e_VarInt:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_int[i] != m_data->m_desc->m_default.m_int)
                    return false;
            } else {
                if (m_data->m_int[i] != 0)
                    return false;
            }
            break;
        case e_VarFloat:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_float[i] != m_data->m_desc->m_default.m_float)
                    return false;
            } else {
                if (m_data->m_float[i] != 0)
                    return false;
            }
            break;
        case e_VarBool:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_bool[i] != m_data->m_desc->m_default.m_bool)
                    return false;
            } else {
                if (m_data->m_bool[i])
                    return false;
            }
            break;
        case e_VarString:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_string[i] != m_data->m_desc->m_default.m_string)
                    return false;
            } else {
                if (!m_data->m_string[i].isNull())
                    return false;
            }
            break;
        case e_VarKey:
            if (!m_data->m_key[i].isNull())
                return false;
            break;
        case e_VarCreatable:
            if (m_data->m_creatable[i] != 0)
                return false;
            break;
        case e_VarDouble:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_double[i] != m_data->m_desc->m_default.m_double)
                    return false;
            } else {
                if (m_data->m_double[i] != 0)
                    return false;
            }
            break;
        case e_VarTime:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_time[i] != m_data->m_desc->m_default.m_time)
                    return false;
            } else {
                if (!m_data->m_time[i].isNull())
                    return false;
            }
            break;
        case e_VarByte:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_byte[i] != m_data->m_desc->m_default.m_int)
                    return false;
            } else {
                if (m_data->m_byte[i] != 0)
                    return false;
            }
            break;
        case e_VarShort:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_short[i] != m_data->m_desc->m_default.m_int)
                    return false;
            } else {
                if (m_data->m_short[i] != 0)
                    return false;
            }
            break;
        case e_VarVector3:
        case e_VarPoint3:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_vector[i] != m_data->m_desc->m_default.m_vector)
                    return false;
            } else {
                if (m_data->m_vector[i].m_X != 0 || m_data->m_vector[i].m_Y != 0
                    || m_data->m_vector[i].m_Z != 0)
                    return false;
            }
            break;
        case e_VarQuaternion:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_quat[i] != m_data->m_desc->m_default.m_quat)
                    return false;
            } else {
                if (m_data->m_quat[i].m_X != 0 || m_data->m_quat[i].m_Y != 0
                    || m_data->m_quat[i].m_Z != 0 || m_data->m_quat[i].m_W != 0)
                    return false;
            }
            break;
        case e_VarRgb:
        case e_VarRgba:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_color[i] != m_data->m_desc->m_default.m_color)
                    return false;
            } else {
                if (m_data->m_color[i].m_R != 0 || m_data->m_color[i].m_G != 0
                    || m_data->m_color[i].m_B != 0 || m_data->m_color[i].m_A != 1)
                    return false;
            }
            break;
        case e_VarRgb8:
        case e_VarRgba8:
            if (m_data->m_desc->m_default.m_valid) {
                if (m_data->m_color8[i] != m_data->m_desc->m_default.m_color8)
                    return false;
            } else {
                if (m_data->m_color8[i].m_R != 0 || m_data->m_color8[i].m_G != 0
                    || m_data->m_color8[i].m_B != 0 || m_data->m_color8[i].m_A != 255)
                    return false;
            }
            break;
        case e_VarStateDesc:
            if (!m_data->m_child[i].isDefault())
                return false;
            break;
        default:
            break;
        }
    }
    return true;
}

SDL::State::State(SDL::StateDescriptor* desc)
    : m_desc(desc), m_flags(0)
{
    if (m_desc) {
        m_vars.resize(desc->m_vars.size());
        m_simpleVars.reserve(desc->m_vars.size());
        m_sdVars.reserve(desc->m_vars.size());
        for (size_t i=0; i<desc->m_vars.size(); ++i) {
            m_vars[i] = SDL::Variable(&m_desc->m_vars[i]);
            if (desc->m_vars[i].m_type == e_VarStateDesc)
                m_sdVars.push_back(&m_vars[i]);
            else
                m_simpleVars.push_back(&m_vars[i]);
        }
        setDefault();
    }
}

enum { e_HFlagVolatile = (1<<0) };

void SDL::State::read(DS::Stream* stream)
{
    m_flags = stream->read<uint16_t>();
    if (stream->read<uint8_t>() != SDL_IOVERSION)
        DS_PASSERT(0);

    size_t count = stupidLengthRead(stream, m_desc->m_vars.size());
    bool useIndices = (count != m_simpleVars.size());
    for (size_t i=0; i<count; ++i) {
        size_t idx = useIndices ? stupidLengthRead(stream, m_desc->m_vars.size()) : i;
        m_simpleVars[idx]->read(stream);
    }

    count = stupidLengthRead(stream, m_desc->m_vars.size());
    useIndices = (count != m_sdVars.size());
    for (size_t i=0; i<count; ++i) {
        size_t idx = useIndices ? stupidLengthRead(stream, m_desc->m_vars.size()) : i;
        m_sdVars[idx]->read(stream);
    }
}

void SDL::State::write(DS::Stream* stream)
{
    // Stream header (see ::Create)
    uint16_t hflags = 0x8000;
    if (!m_object.isNull())
        hflags |= e_HFlagVolatile;
    stream->write<uint16_t>(hflags);
    stream->writeSafeString(m_desc->m_name);
    stream->write<int16_t>(m_desc->m_version);

    if (!m_object.isNull())
        m_object.write(stream);

    // State data
    stream->write<uint16_t>(m_flags);
    stream->write<uint8_t>(SDL_IOVERSION);

    size_t count = 0;
    for (size_t i=0; i<m_simpleVars.size(); ++i) {
        if (m_simpleVars[i]->data()->m_flags & Variable::e_XIsDirty)
            ++count;
    }
    stupidLengthWrite(stream, m_desc->m_vars.size(), count);
    bool useIndices = (count != m_simpleVars.size());
    for (size_t i=0; i<m_simpleVars.size(); ++i) {
        if (m_simpleVars[i]->data()->m_flags & Variable::e_XIsDirty) {
            if (useIndices)
                stupidLengthWrite(stream, m_desc->m_vars.size(), i);
            m_simpleVars[i]->write(stream);
        }
    }

    count = 0;
    for (size_t i=0; i<m_sdVars.size(); ++i) {
        if (m_sdVars[i]->data()->m_flags & Variable::e_XIsDirty)
            ++count;
    }
    stupidLengthWrite(stream, m_desc->m_vars.size(), count);
    useIndices = (count != m_sdVars.size());
    for (size_t i=0; i<count; ++i) {
        if (m_sdVars[i]->data()->m_flags & Variable::e_XIsDirty) {
            if (useIndices)
                stupidLengthWrite(stream, m_desc->m_vars.size(), i);
            m_sdVars[i]->write(stream);
        }
    }
}

void SDL::State::setDefault()
{
    for (std::vector<Variable>::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
        it->setDefault();
}

bool SDL::State::isDefault() const
{
    for (std::vector<Variable>::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it) {
        if (!it->isDefault())
            return false;
    }
    return true;
}

bool SDL::State::isDirty() const
{
    for (std::vector<Variable>::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it) {
        if (it->descriptor()->m_type == e_VarStateDesc) {
            for (size_t i=0; i<it->data()->m_size; ++i) {
                if (it->data()->m_child[i].isDirty())
                    return true;
            }
        } else {
            if (it->data()->m_flags & Variable::e_XIsDirty)
                return true;
        }
    }
    return false;
}

SDL::State SDL::State::Create(DS::Stream* stream)
{
    uint16_t flags = stream->read<uint16_t>();
    DS_DASSERT((flags & 0x8000) != 0);

    DS::String name = stream->readSafeString();
    int version = stream->read<int16_t>();
    SDL::StateDescriptor* desc = SDL::DescriptorDb::FindDescriptor(name, version);
    State state(desc);

    if (flags & e_HFlagVolatile)
        state.m_object.read(stream);
    return state;
}
