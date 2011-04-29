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

#ifndef _MOUL_CREATABLE_H
#define _MOUL_CREATABLE_H

#include "config.h"
#include "streams.h"

#define FACTORY_CREATABLE(type) \
    protected: friend class Factory; \
    public: static type* Create() { return new type(ID_##type); }

namespace MOUL
{
    enum CreatableTypes
    {
    /* THAR BE MAJICK HERE */
    #define CREATABLE_TYPE(id, cre) \
        ID_##cre = id,
    #include "creatable_types.inl"
    #undef CREATABLE_TYPE
    };

    class Creatable
    {
    public:
        template <class cre_t> cre_t* Cast()
        { return dynamic_cast<cre_t*>(this); }

        template <class cre_t> const cre_t* Cast() const
        { return dynamic_cast<const cre_t*>(this); }

        uint16_t type() const { return m_type; }

        void ref() { __sync_fetch_and_add(&m_refs, 1); }

        void unref()
        {
            if (this && __sync_add_and_fetch(&m_refs, -1) == 0)
                delete this;
        }

        virtual void read(DS::Stream* stream) { }
        virtual void write(DS::Stream* stream) { }

    protected:
        /* Only the Factory may create creatables */
        Creatable(uint16_t type) : m_refs(1), m_type(type) { }
        friend class Factory;

        /* Prevent clients from directly destroying creatables */
        virtual ~Creatable() { }

    private:
        int m_refs;
        uint16_t m_type;

        /* Prevent stack copying of creatables */
        Creatable(const Creatable& copy) { }
        Creatable& operator=(const Creatable& copy) { return *this; }
    };
}

#endif
