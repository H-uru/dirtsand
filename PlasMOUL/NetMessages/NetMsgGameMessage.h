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

#ifndef _MOUL_NETMSGGAMEMESSAGE_H
#define _MOUL_NETMSGGAMEMESSAGE_H

#include "NetMsgObject.h"
#include "Messages/Message.h"

namespace MOUL
{
    class NetMsgGameMessage : public NetMessage
    {
        FACTORY_CREATABLE(NetMsgGameMessage)

        NetMsgStream::Compression m_compression;
        MOUL::Message* m_message;
        DS::UnifiedTime m_deliveryTime;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        NetMsgGameMessage(uint16_t type)
            : NetMessage(type), m_compression(NetMsgStream::e_CompressNone),
              m_message() { }

        virtual ~NetMsgGameMessage() { m_message->unref(); }
    };

    class NetMsgGameMessageDirected : public NetMsgGameMessage
    {
        FACTORY_CREATABLE(NetMsgGameMessageDirected)

        std::vector<uint32_t> m_receivers;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        NetMsgGameMessageDirected(uint16_t type) : NetMsgGameMessage(type) { }
    };
}

#endif
