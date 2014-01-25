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

#ifndef _MOUL_AVATARINPUTSTATEMSG_H
#define _MOUL_AVATARINPUTSTATEMSG_H

#include "Message.h"

namespace MOUL
{
    class AvatarInputStateMsg : public Message
    {
        FACTORY_CREATABLE(AvatarInputStateMsg)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    public:
        uint16_t m_state;

    protected:
        AvatarInputStateMsg(uint16_t type) : Message(type), m_state() { }
    };
}

#endif
