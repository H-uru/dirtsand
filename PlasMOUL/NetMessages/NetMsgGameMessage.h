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

#ifndef _MOUL_NETMSGGAMEMESSAGE_H
#define _MOUL_NETMSGGAMEMESSAGE_H

#include "NetMessage.h"
#include "Messages/Message.h"
#include "Key.h"

namespace MOUL
{
    class NetMsgStream
    {
    public:
        enum Compression {
            e_CompressNone, e_CompressFail, e_CompressZlib, e_CompressNever
        };

        NetMsgStream(Compression compress = e_CompressNone)
            : m_compression(compress) { }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

        Compression m_compression;
        DS::BufferStream m_stream;
    };

    class NetMsgGameMessage : public NetMessage
    {
        FACTORY_CREATABLE(NetMsgGameMessage)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    protected:
        NetMsgGameMessage(uint16_t type)
            : NetMessage(type), m_compression(NetMsgStream::e_CompressNone),
              m_message(0) { }

    public:
        NetMsgStream::Compression m_compression;
        MOUL::Message* m_message;
        DS::UnifiedTime m_deliveryTime;
    };
}

#endif
