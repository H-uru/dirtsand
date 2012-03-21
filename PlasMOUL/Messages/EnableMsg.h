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

#ifndef _MOUL_ENABLEMSG_H
#define _MOUL_ENABLEMSG_H

#include "Message.h"
#include "Types/BitVector.h"

namespace MOUL
{
    class EnableMsg : public Message
    {
        FACTORY_CREATABLE(EnableMsg)

        enum Commands
        {
            kDisable, kEnable, kDrawable,
            kPhysical, kAudible, kAll, kByType
        };

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    public:
        DS::BitVector m_cmd;
        DS::BitVector m_types;

    protected:
        EnableMsg(uint16_t type) : Message(type) { }
    };
}

#endif
