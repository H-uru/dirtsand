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

#ifndef _MOUL_MULTISTAGEMSG_H
#define _MOUL_MULTISTAGEMSG_H

#include "Message.h"
#include "Types/BitVector.h"

namespace MOUL
{
    class MultistageModMsg : public Message
    {
        FACTORY_CREATABLE(MultistageModMsg);

        virtual void read(DS::Stream* s);
        virtual void write(DS::Stream* s) const;

    public:
        DS::BitVector m_cmds;
        uint8_t m_stage;
        uint8_t m_numLoops;

    protected:
        MultistageModMsg(uint16_t type)
            : Message(type), m_stage(0), m_numLoops(0) { }
    };
};

#endif // _MOUL_MULTISTAGEMSG_H
