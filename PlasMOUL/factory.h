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

#ifndef _MOUL_FACTORY_H
#define _MOUL_FACTORY_H

#include <exception>
#include "creatable.h"

namespace MOUL
{
    class Factory
    {
    public:
        static Creatable* Create(uint16_t type);
    };

    class FactoryException : public std::exception
    {
    public:
        virtual const char* what() const throw()
        { return "FactoryException: Unknown creatable ID"; }
    };
}

#endif
