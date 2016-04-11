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

#ifndef _MOUL_INPUTEVENTMSG_H
#define _MOUL_INPUTEVENTMSG_H

#include "Message.h"
#include "Types/Math.h"

namespace MOUL
{
    class InputEventMsg : public Message
    {
        FACTORY_CREATABLE(InputEventMsg)

        int32_t m_event;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

        virtual bool makeSafeForNet() { return false; }

    protected:
        InputEventMsg(uint16_t type) : Message(type), m_event() { }
    };

    class ControlEventMsg : public InputEventMsg
    {
        FACTORY_CREATABLE(ControlEventMsg)

        int32_t m_controlCode;
        bool m_activated;
        float m_controlPercent;
        DS::Vector3 m_turnToPoint;
        ST::string m_cmd;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        ControlEventMsg(uint16_t type)
            : InputEventMsg(type), m_controlCode(), m_activated(),
              m_controlPercent() { }
    };
};

#endif
