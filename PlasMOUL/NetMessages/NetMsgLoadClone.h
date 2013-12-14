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

#ifndef _MOUL_NETMSGLOADCLONE_H
#define _MOUL_NETMSGLOADCLONE_H

#include "NetMsgGameMessage.h"
#include "Key.h"

namespace MOUL
{
    class NetMsgLoadClone : public NetMsgGameMessage
    {
        FACTORY_CREATABLE(NetMsgLoadClone)

        MOUL::Uoid m_object;
        bool m_isPlayer, m_isLoading, m_isInitialState;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        NetMsgLoadClone(uint16_t type) : NetMsgGameMessage(type) { }
    };
}

#endif
