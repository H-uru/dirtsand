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

#ifndef _MOUL_LINKTOAGEMSG_H
#define _MOUL_LINKTOAGEMSG_H

#include "AgeLinkStruct.h"
#include "Message.h"

namespace MOUL
{
    class LinkToAgeMsg : public Message
    {
        FACTORY_CREATABLE(LinkToAgeMsg)

        virtual void read(DS::Stream* s);
        virtual void write(DS::Stream* s) const;

        virtual bool makeSafeForNet();

    public:
        AgeLinkStruct* m_ageLink;
        DS::String m_linkInAnim;
        uint8_t m_flags;

    protected:
        LinkToAgeMsg(uint16_t type)
            : Message(type), m_ageLink(AgeLinkStruct::Create()) { }

        virtual ~LinkToAgeMsg()
        {
            m_ageLink->unref();
        }
    };
};

#endif // _MOUL_LINKTOAGEMSG_H
