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

#ifndef _DS_ERRORS_H
#define _DS_ERRORS_H

#include "errno.h"
#include <exception>

namespace DS
{
    class AssertException : public std::exception
    {
    public:
        AssertException(const char* cond, const char* file, long line) throw()
            : m_cond(cond), m_file(file), m_line(line) { }
        virtual ~AssertException() throw() { }

        virtual const char* what() const throw()
        { return "[AssertException] Assertion Failed"; }

    public:
        const char* m_cond;
        const char* m_file;
        long m_line;
    };
}

#define DS_PASSERT(cond) \
    if (!(cond)) throw DS::AssertException(#cond, __FILE__, __LINE__)

#ifdef DEBUG
#define DS_DASSERT(cond) DS_PASSERT(cond)
#else
#define DS_DASSERT(cond)
#endif

#endif
