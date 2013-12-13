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

#ifndef _MOUL_FACTORY_H
#define _MOUL_FACTORY_H

#include <exception>
#include "creatable.h"

#ifdef DEBUG
#include <typeinfo>
#endif

namespace MOUL
{
    class Factory
    {
    public:
        static Creatable* Create(uint16_t type);

        static Creatable* ReadCreatable(DS::Stream* stream);
        static void WriteCreatable(DS::Stream* stream, const Creatable* obj);

        template <class cre_t>
        static cre_t* Read(DS::Stream* stream)
        {
            Creatable* obj = ReadCreatable(stream);
            if (obj) {
                cre_t* result = obj->Cast<cre_t>();
#ifdef DEBUG
                if (!result) {
                    fputs("Error: Cast did not match expected type\n", stderr);
                    fprintf(stderr, "    FROM %s ; TO %s\n",
                            typeid(*obj).name(), typeid(cre_t).name());
                }
#endif
                if (!result)
                    obj->unref();
                return result;
            } else {
                return 0;
            }
        }
    };

    class FactoryException : public std::exception
    {
    public:
        enum Type { e_UnknownType };

        FactoryException(Type type) throw() : m_type(type) { }
        virtual ~FactoryException() throw() { }

        virtual const char* what() const throw()
        {
            static const char* _messages[] = {
                "[FactoryException] Unknown creatable ID"
            };
            return _messages[m_type];
        }

    private:
        Type m_type;
    };
}

#endif
