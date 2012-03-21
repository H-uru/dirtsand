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

#ifndef _MOUL_NETMSGSDLSTATE_H
#define _MOUL_NETMSGSDLSTATE_H

#include "NetMsgObject.h"

namespace MOUL
{
    class NetMsgSDLState : public NetMsgObject
    {
        FACTORY_CREATABLE(NetMsgSDLState)

        DS::Blob m_sdlBlob;
        bool m_isInitial, m_persistOnServer, m_isAvatar;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    protected:
        NetMsgSDLState(uint16_t type) : NetMsgObject(type) { }
    };

    class NetMsgSDLStateBCast : public NetMsgSDLState
    {
        FACTORY_CREATABLE(NetMsgSDLStateBCast)

    protected:
        NetMsgSDLStateBCast(uint16_t type) : NetMsgSDLState(type) { }
    };
}

#endif
