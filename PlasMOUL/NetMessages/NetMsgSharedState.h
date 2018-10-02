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

#ifndef _MOUL_NETMSGSHAREDSTATE_H
#define _MOUL_NETMSGSHAREDSTATE_H

#include "NetMsgObject.h"

namespace MOUL
{
    struct GenericType
    {
        enum Type
        {
            e_TypeInt, e_TypeFloat, e_TypeBool, e_TypeString, e_TypeByte,
            e_TypeAny, e_TypeUint, e_TypeDouble, e_TypeNone = 0xFF,
        };

        union
        {
            int32_t  m_int;
            uint32_t m_uint;
            float    m_float;
            double   m_double;
            bool     m_bool;
            uint8_t  m_byte;
        };
        ST::string   m_string;
        uint8_t      m_type;

        GenericType() : m_type(e_TypeNone) { }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream) const;
    };

    struct GenericVar
    {
        ST::string  m_name;
        GenericType m_value;

        void read(DS::Stream* stream);
        void write(DS::Stream* stream) const;
    };

    class NetMsgSharedState : public NetMsgObject
    {
    public:
        uint8_t m_lockRequest;

        // Shared state info
        bool m_serverMayDelete;
        ST::string m_stateName;
        std::vector<GenericVar> m_vars;

        void read(DS::Stream* stream) override;
        void write(DS::Stream* stream) const override;

    protected:
        NetMsgSharedState(uint16_t type)
            : NetMsgObject(type), m_lockRequest(), m_serverMayDelete() { }
    };

    class NetMsgTestAndSet : public NetMsgSharedState
    {
        FACTORY_CREATABLE(NetMsgTestAndSet)

    protected:
        NetMsgTestAndSet(uint16_t type) : NetMsgSharedState(type) { }
    };
}

#endif
