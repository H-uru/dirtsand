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

#ifndef _MOUL_AVTASKMSG_H
#define _MOUL_AVTASKMSG_H

#include "AvatarMsg.h"
#include "Avatar/AvTask.h"

namespace MOUL
{
    class AvTaskMsg : public AvatarMsg
    {
        FACTORY_CREATABLE(AvTaskMsg)

        AvTask* m_task;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        AvTaskMsg(uint16_t type) : AvatarMsg(type), m_task() { }

        virtual ~AvTaskMsg() { m_task->unref(); }
    };

    class AvPushBrainMsg : public AvTaskMsg
    {
        FACTORY_CREATABLE(AvPushBrainMsg)

        ArmatureBrain* m_brain;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        AvPushBrainMsg(uint16_t type) : AvTaskMsg(type), m_brain() { }

        virtual ~AvPushBrainMsg() { m_brain->unref(); }
    };

    class AvPopBrainMsg : public AvTaskMsg
    {
        FACTORY_CREATABLE(AvPopBrainMsg)

    protected:
        AvPopBrainMsg(uint16_t type) : AvTaskMsg(type) { }
    };
}

#endif
