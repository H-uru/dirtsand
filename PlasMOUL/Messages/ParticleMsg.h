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

#ifndef _MOUL_PARTICLEMSG_H
#define _MOUL_PARTICLEMSG_H

#include "Message.h"

namespace MOUL
{
    class ParticleKillMsg : public Message
    {
        FACTORY_CREATABLE(ParticleKillMsg)

        virtual void read(DS::Stream* s);
        virtual void write(DS::Stream* s);

    public:
        enum
        {
            e_ParticleKillImmortalOnly = 1<<0,
            e_ParticleKillPercentage = 1<<1,
        };

        uint8_t m_flags;
        float m_numToKill, m_timeLeft;

    protected:
        ParticleKillMsg(uint16_t type)
            : Message(type), m_flags(0), m_numToKill(0.f), m_timeLeft(0.f) { }
    };

    class ParticleTransferMsg : public Message
    {
        FACTORY_CREATABLE(ParticleTransferMsg)

        virtual void read(DS::Stream* s);
        virtual void write(DS::Stream* s);

    public:
        Key m_sysKey;
        uint16_t m_transferCount;

    protected:
        ParticleTransferMsg(uint16_t type)
            : Message(type), m_transferCount(0) { }
    };
};

#endif // _MOUL_PARTICLEMSG_H
