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

#ifndef _MOUL_NOTIFYMSG_H
#define _MOUL_NOTIFYMSG_H

#include "Message.h"
#include "EventData.h"
#include <vector>

namespace MOUL
{
    class NotifyMsg : public Message
    {
        FACTORY_CREATABLE(NotifyMsg)

        enum Type
        {
            e_Activator, e_VarNotification, e_NotifySelf, e_ResponderFastFwd,
            e_ResponderChangeState,
        };

        Type m_type;
        int32_t m_id;
        float m_state;
        std::vector<EventData*> m_events;

        void read(DS::Stream* stream) override;
        void write(DS::Stream* stream) const override;

    protected:
        NotifyMsg(uint16_t type) : Message(type), m_type(), m_id(), m_state() { }
        ~NotifyMsg() override;
    };
}

#endif
