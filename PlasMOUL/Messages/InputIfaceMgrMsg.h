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

#ifndef _MOUL_INPUTIFACEMGRMSG_H
#define _MOUL_INPUTIFACEMGRMSG_H

#include "Message.h"
#include "Key.h"

namespace MOUL
{
    class InputIfaceMgrMsg : public Message
    {
        FACTORY_CREATABLE(InputIfaceMgrMsg)

        uint8_t m_command;
        uint32_t m_pageId;
        DS::String m_ageName, m_ageFilename, m_spawnPoint;
        Key m_avatar;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        InputIfaceMgrMsg(uint16_t type)
            : Message(type), m_command(0), m_pageId(0) { }
    };
}

#endif
