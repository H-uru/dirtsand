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

#ifndef _MOUL_NETMSGSHAREDSTATE_H
#define _MOUL_NETMSGSHAREDSTATE_H

#include "NetMsgObject.h"

namespace MOUL
{
    class NetMsgSharedState : public NetMsgObject
    {
    public:
        uint8_t m_lockRequest;
    private:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    protected:
        NetMsgSharedState(uint16_t type)
            : NetMsgObject(type), m_lockRequest(0) { }
    };

    class NetMsgTestAndSet : public NetMsgSharedState
    {
        FACTORY_CREATABLE(NetMsgTestAndSet)

    protected:
        NetMsgTestAndSet(uint16_t type) : NetMsgSharedState(type) { }
    };
}

#endif
