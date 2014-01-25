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

#ifndef _MOUL_AVSEEKMSG_H
#define _MOUL_AVSEEKMSG_H

#include "AvTaskMsg.h"
#include "Key.h"
#include "Types/Math.h"

namespace MOUL
{
    class AvSeekMsg : public AvTaskMsg
    {
        FACTORY_CREATABLE(AvSeekMsg)

        Key m_seekPoint;
        DS::Vector3 m_targetPos, m_targetLook;
        float m_duration;
        bool m_smartSeek, m_noSeek;
        uint16_t m_alignType;
        uint8_t m_flags;
        DS::String m_animName;
        Key m_finishKey;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        AvSeekMsg(uint16_t type)
            : AvTaskMsg(type), m_duration(), m_smartSeek(), m_noSeek(),
              m_alignType(), m_flags() { }
    };

    class AvOneShotMsg : public AvSeekMsg
    {
        FACTORY_CREATABLE(AvOneShotMsg)

        DS::String m_oneShotAnimName;
        bool m_drivable, m_reversible;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        AvOneShotMsg(uint16_t type)
            : AvSeekMsg(type), m_drivable(), m_reversible() { }
    };
}

#endif
