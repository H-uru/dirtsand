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

#ifndef _MOUL_ANIMSTAGE_H
#define _MOUL_ANIMSTAGE_H

#include "creatable.h"

namespace MOUL
{
    class AnimStage : public Creatable
    {
        FACTORY_CREATABLE(AnimStage)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

        void readAux(DS::Stream* stream);
        void writeAux(DS::Stream* stream);

    public:
        DS::String m_animName;
        uint8_t m_notify;
        uint32_t m_forwardType, m_backType;
        uint32_t m_advanceType, m_regressType;
        uint32_t m_loops;
        bool m_doAdvance, m_doRegress;
        uint32_t m_advanceTo, m_regressTo;

        float m_localTime, m_length;
        int32_t m_curLoop;
        bool m_attached;

    protected:
        AnimStage(uint16_t type) : Creatable(type) { }
    };
}

#endif
