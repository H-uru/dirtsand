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

#include <string_theory/stdio>
#include <cstdlib>
#include <stdexcept>

namespace DS
{
    __attribute__((noreturn))
    inline void AssertionFailure(const char *condition, const char *file, long line)
        noexcept
    {
        ST::printf(stderr, "FATAL: Assertion Failure at {}:{}: {}\n",
                   file, line, condition);
        // Exit code 3 not used anywhere else...
        exit(3);
    }

    class MalformedData : public std::runtime_error
    {
    public:
        MalformedData()
            : std::runtime_error("Malformed stream data from client") { }
    };

    class InvalidConnectionHeader : public std::runtime_error
    {
    public:
        InvalidConnectionHeader()
            : std::runtime_error("Invalid connection header received from client") { }
    };

    // A "catchable" system error
    class SystemError : public std::runtime_error
    {
    public:
        explicit SystemError(const char *message)
            : std::runtime_error(std::string(message))
        { }

        SystemError(const char *message, const char *error)
            : std::runtime_error(std::string(message) + ": " + std::string(error))
        { }
    };
}

#define DS_ASSERT(cond) \
    if (!(cond)) DS::AssertionFailure(#cond, __FILE__, __LINE__)

#ifdef DEBUG
#define DEBUG_printf(...)   ST::printf(__VA_ARGS__)
#else
#define DEBUG_printf(...)   ((void)0)
#endif

#endif
