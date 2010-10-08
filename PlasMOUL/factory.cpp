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

#include "factory.h"

#include "Messages/LoadCloneMsg.h"

MOUL::Creatable* MOUL::Factory::Create(uint16_t type)
{
    switch (type) {
/* THAR BE MAJICK HERE */
#define CREATABLE_TYPE(id, cre) \
    case id: return new cre(id);
#include "creatable_types.h"
#undef CREATABLE_TYPE
    case 0x8000: return static_cast<Creatable*>(0);
    default:
        throw FactoryException();
    }
}
