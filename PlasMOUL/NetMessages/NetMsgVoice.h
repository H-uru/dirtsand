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

#ifndef _MOUL_NETMSGVOICE_H
#define _MOUL_NETMSGVOICE_H

#include "NetMessage.h"
#include <vector>

namespace MOUL
{
    class NetMsgVoice : public NetMessage
    {
        FACTORY_CREATABLE(NetMsgVoice)

        uint8_t m_flags;
        uint8_t m_frames;
        DS::Blob m_data;
        std::vector<uint32_t> m_receivers;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        NetMsgVoice(uint16_t type)
            : NetMessage(type), m_flags(), m_frames() { }
    };
}

#endif
