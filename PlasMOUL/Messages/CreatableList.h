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

#ifndef _MOUL_CREATABLELIST_H
#define _MOUL_CREATABLELIST_H

#include "creatable.h"
#include <map>
#include <vector>

namespace MOUL
{
    class CreatableList
    {
        enum Flags
        {
            e_WantCompression = (1<<0),
            e_Compressed      = (1<<1),
            e_Written         = (1<<2), // Only used in the client
        };

        uint8_t m_flags;
        std::map<uint16_t, Creatable*> m_items;

    public:
        CreatableList() : m_flags(e_WantCompression) { }
        virtual ~CreatableList() { clear(); }

        void clear();

        void read(DS::Stream* stream);
        void write(DS::Stream* stream) const;
    };
}

#endif // _MOUL_CREATABLELIST_H
