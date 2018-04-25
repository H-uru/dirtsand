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

#include "GenericValue.h"
#include "errors.h"

void MOUL::GenericValue::read(DS::Stream* stream)
{
    m_dataType = stream->read<uint8_t>();

    switch (m_dataType)
    {
    case e_String:
    case e_Any:
        m_string = stream->readSafeString();
        break;
    case e_Bool:
        m_bool = stream->read<bool>();
        break;
    case e_Char:
        m_char = stream->read<char>();
        break;
    case e_Int:
        m_int = stream->read<int32_t>();
        break;
    case e_UInt:
        m_uint = stream->read<uint32_t>();
        break;
    case e_Float:
        m_float = stream->read<float>();
        break;
    case e_Double:
        m_double = stream->read<double>();
        break;
    case e_None:
        break;
    }
}

void MOUL::GenericValue::write(DS::Stream* stream) const
{
    stream->write<uint8_t>(m_dataType);

    switch (m_dataType)
    {
    case e_String:
    case e_Any:
        stream->writeSafeString(m_string);
        break;
    case e_Bool:
        stream->write<bool>(m_bool);
        break;
    case e_Char:
        stream->write<char>(m_char);
        break;
    case e_Int:
        stream->write<int32_t>(m_int);
        break;
    case e_UInt:
        stream->write<uint32_t>(m_uint);
        break;
    case e_Float:
        stream->write<float>(m_float);
        break;
    case e_Double:
        stream->write<double>(m_double);
        break;
    case e_None:
        break;
    }
}
