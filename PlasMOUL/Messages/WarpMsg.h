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

#ifndef _MOUL_WARPMSG_H
#define _MOUL_WARPMSG_H

#include "Message.h"
#include "Types/Math.h"

namespace MOUL
{
    class WarpMsg : public Message
    {
        FACTORY_CREATABLE(WarpMsg)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

        virtual bool makeSafeForNet() { return false; }

    public:
        DS::Matrix44 m_transform;
        uint32_t m_warpFlags;

    protected:
        WarpMsg(uint16_t type) : Message(type), m_warpFlags() { }
    };
}

#endif // _MOUL_WARPMSG_H
