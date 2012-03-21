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

#ifndef _MOUL_NETMSGROOMSLIST_H
#define _MOUL_NETMSGROOMSLIST_H

#include "NetMessage.h"
#include "Key.h"
#include <vector>

namespace MOUL
{
    class NetMsgRoomsList : public NetMessage
    {
    public:
        struct Room
        {
            Location m_location;
            DS::String m_name;
        };

        std::vector<Room> m_rooms;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    protected:
        NetMsgRoomsList(uint16_t type) : NetMessage(type) { }
    };

    class NetMsgPagingRoom : public NetMsgRoomsList
    {
        FACTORY_CREATABLE(NetMsgPagingRoom)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

        enum PagingFlags
        {
            e_PagingOut      = (1<<0),
            e_ResetList      = (1<<1),
            e_RequestState   = (1<<2),
            e_FinalRoomInAge = (1<<3),
        };

        uint8_t m_pagingFlags;

    protected:
        NetMsgPagingRoom(uint16_t type) : NetMsgRoomsList(type) { }
    };
}

#endif
