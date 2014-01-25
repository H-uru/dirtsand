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

#ifndef _MOUL_LINKINGMGRMSG_H
#define _MOUL_LINKINGMGRMSG_H

#include "Message.h"
#include "Types/BitVector.h"
#include "CreatableList.h"

namespace MOUL
{
    class LinkingMgrMsg : public Message
    {
        FACTORY_CREATABLE(LinkingMgrMsg)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

        virtual bool makeSafeForNet() { return false; }

    public:
        uint8_t m_cmd;
        DS::BitVector m_contentFlags;
        CreatableList m_args;

    protected:
        LinkingMgrMsg(uint16_t type) : Message(type), m_cmd() { }
    };
}

#endif // _MOUL_LINKINGMGRMSG_H
