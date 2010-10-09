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

#ifndef _MOUL_CONFIG_H
#define _MOUL_CONFIG_H

#include <cstdlib>

typedef unsigned char       uint8_t;
typedef signed char         sint8_t;
typedef unsigned short      uint16_t;
typedef signed short        sint16_t;
typedef unsigned int        uint32_t;
typedef signed int          sint32_t;
typedef unsigned long long  uint64_t;
typedef signed long long    sint64_t;
typedef unsigned char       chr8_t;
typedef unsigned short      chr16_t;

typedef union
{
    uint32_t uint;
    sint32_t sint;
    chr8_t*  cstring;
    chr16_t* wstring;
    void*    data;
} msgparm_t;

#endif
