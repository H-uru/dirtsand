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

#ifndef _MOUL_AVATARMSG_H
#define _MOUL_AVATARMSG_H

#include "Message.h"

namespace MOUL
{
    class AvatarMsg : public Message
    {
    protected:
        AvatarMsg(uint16_t type) : Message(type) { }
    };

    class AvBrainGenericMsg : public AvatarMsg
    {
        FACTORY_CREATABLE(AvBrainGenericMsg)

    public:
        enum Type
        {
            e_NextStage, e_PrevStage, e_GotoStage, e_SetLoopCount
        };

        Type m_type;
        int32_t m_stage;
        float m_newTime, m_transitionTime;
        bool m_newDirection;
        bool m_setTime, m_setDirection;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    protected:
        AvBrainGenericMsg(uint16_t type) : AvatarMsg(type) { }
    };

    class AvTaskSeekDoneMsg : public AvatarMsg
    {
        FACTORY_CREATABLE(AvTaskSeekDoneMsg)

        bool m_aborted;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    protected:
        AvTaskSeekDoneMsg(uint16_t type) : AvatarMsg(type) { }
    };
}

#endif
