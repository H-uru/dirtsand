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

#ifndef _MOUL_MESSAGEWITHCALLBACKS_H
#define _MOUL_MESSAGEWITHCALLBACKS_H

#include "Message.h"
#include "Types/BitVector.h"
#include <vector>

namespace MOUL
{
    class MessageWithCallbacks : public Message
    {
        FACTORY_CREATABLE(MessageWithCallbacks)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

        virtual bool makeSafeForNet();

    public:
        std::vector<Message*> m_callbacks;

    protected:
        MessageWithCallbacks(uint16_t type)
            : Message(type) { }

        virtual ~MessageWithCallbacks();
    };

    class AnimCmdMsg : public MessageWithCallbacks
    {
        FACTORY_CREATABLE(AnimCmdMsg)

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

    public:
        enum
        {
            e_Continue, e_Stop, e_SetLooping, e_UnSetLooping, e_SetBegin,
            e_SetEnd, e_SetLoopEnd, e_SetLoopBegin, e_SetSpeed, e_GoToTime,
            e_SetBackwards, e_SetForewards, e_ToggleState, e_AddCallbacks,
            e_RemoveCallbacks, e_GoToBegin, e_GoToEnd, e_GoToLoopBegin,
            e_GoToLoopEnd, e_IncrementForward, e_IncrementBackward,
            e_RunForward, e_RunBackward, e_PlayToTime, e_PlayToPercentage, 
            e_FastForward, e_GoToPercent
        };

        DS::BitVector m_cmd;
        float m_begin, m_end, m_loopEnd, m_loopBegin, m_speed, m_speedChangeRate, m_time;
        DS::String m_animName, m_loopName;

    protected:
        AnimCmdMsg(uint16_t type)
            : MessageWithCallbacks(type), m_begin(0.0), m_end(0.0),
              m_loopEnd(0.0), m_loopBegin(0.0), m_speed(0.0),
              m_speedChangeRate(0.0), m_time(0.0) { }
    };
};

#endif // _MOUL_MESSAGEWITHCALLBACKS_H
