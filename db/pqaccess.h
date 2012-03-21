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

#include "Types/Uuid.h"
#include <libpq-fe.h>

template <size_t count>
struct PostgresStrings
{
    const char* m_values[count];
    DS::String m_strings[count];

    void set(size_t idx, const DS::String& str)
    {
        m_strings[idx] = str;
        this->m_values[idx] = str.c_str();
    }

    void set(size_t idx, uint32_t value)
    {
        m_strings[idx] = DS::String::Format("%u", value);
        this->m_values[idx] = m_strings[idx].c_str();
    }

    void set(size_t idx, int value)
    {
        m_strings[idx] = DS::String::Format("%d", value);
        this->m_values[idx] = m_strings[idx].c_str();
    }
};
