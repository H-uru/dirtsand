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

#ifndef _MOUL_GENERICVALUE_H
#define _MOUL_GENERICVALUE_H

#include "creatable.h"

namespace MOUL
{
    class GenericValue : public Creatable
    {
        FACTORY_CREATABLE(GenericValue)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    public:
        enum Types
        {
            e_Int,
            e_Float,
            e_Bool,
            e_String,
            e_Char,
            e_Any,
            e_UInt,
            e_Double,
            e_None = 0xFF,
        };

        void set(int32_t i) { m_int = i; m_dataType = e_Int; }
        void set(uint32_t i) { m_uint = i; m_dataType = e_UInt; }
        void set(float f) { m_float = f; m_dataType = e_Float; }
        void set(double d) { m_double = d; m_dataType = e_Double; }
        void set(bool b) { m_bool = b; m_dataType = e_Bool; }
        void set(const DS::String& s) { m_string = s; m_dataType = e_String; }
        void set(char c) { m_char = c; m_dataType = e_Char; }
        void setAny(const DS::String& s) { m_string = s; m_dataType = e_Any; }
        void reset() { m_dataType = e_None; }

        int32_t toInt() const;
        uint32_t toUint() const;
        float toFloat() const;
        double toDouble() const;
        bool toBool() const;
        DS::String toString() const;
        char toChar() const;

    protected:
        GenericValue(uint16_t type) : Creatable(type), m_dataType(e_None) { }

        uint8_t m_dataType;
        DS::String m_string;

        union
        {
            int32_t m_int;
            uint32_t m_uint;
            float m_float;
            double m_double;
            bool m_bool;
            char m_char;
        };
    };
}

#endif
