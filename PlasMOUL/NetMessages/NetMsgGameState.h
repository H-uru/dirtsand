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

#ifndef _MOUL_NETMSGGAMESTATE_H
#define _MOUL_NETMSGGAMESTATE_H

#include "NetMsgRoomsList.h"

namespace MOUL
{
    class NetMsgGameStateRequest : public NetMsgRoomsList
    {
        FACTORY_CREATABLE(NetMsgGameStateRequest)

    protected:
        NetMsgGameStateRequest(uint16_t type) : NetMsgRoomsList(type) { }
    };

    class NetMsgInitialAgeStateSent : public NetMsgServerToClient
    {
        FACTORY_CREATABLE(NetMsgInitialAgeStateSent)

        uint32_t m_numStates;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        NetMsgInitialAgeStateSent(uint16_t type) : NetMsgServerToClient(type) { }
    };
}

#endif
