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

#ifndef _MOUL_BULLETMSG_H
#define _MOUL_BULLETMSG_H

#include "Message.h"
#include "Types/Math.h"

namespace MOUL
{
    class BulletMsg : public Message
    {
        FACTORY_CREATABLE(BulletMsg)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    public:
        enum { e_Stop, e_Shot, e_Spray };

        uint8_t m_cmd;
        DS::Vector3 m_from, m_direction;
        float m_range, m_radius, m_partyTime;

    protected:
        BulletMsg(uint16_t type)
            : Message(type), m_cmd(0), m_range(0.0), 
              m_radius(0.0), m_partyTime(0.0) { }
    };
};

#endif // _MOUL_BULLETMSG_H
