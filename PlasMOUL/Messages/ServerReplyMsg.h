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

#ifndef _MOUL_SERVERREPLYMSG_H
#define _MOUL_SERVERREPLYMSG_H

#include "Message.h"

namespace MOUL
{
    class ServerReplyMsg : public Message
    {
        FACTORY_CREATABLE(ServerReplyMsg)

        enum Type { e_Invalid = -1, e_Deny, e_Affirm };
        Type m_reply;

        void read(DS::Stream* stream) override;
        void write(DS::Stream* stream) const override;

    protected:
        ServerReplyMsg(uint16_t type) : Message(type), m_reply(e_Invalid) { }
    };
}

#endif
