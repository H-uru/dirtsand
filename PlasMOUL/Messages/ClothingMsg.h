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

#ifndef _MOUL_CLOTHINGMSG_H
#define _MOUL_CLOTHINGMSG_H

#include "Message.h"
#include "Types/Color.h"

namespace MOUL
{
    class ClothingMsg : public Message
    {
        FACTORY_CREATABLE(ClothingMsg)

        enum Commands
        {
            e_AddItem            = (1<<0),
            e_RemoveItem         = (1<<1),
            e_UpdateTexture      = (1<<2),
            e_TintItem           = (1<<3),
            e_Retry              = (1<<4),
            e_TintSkin           = (1<<5),
            e_BlendSkin          = (1<<6),
            e_MorphItem          = (1<<7),
            e_SaveCustomizations = (1<<8),
        };

        uint32_t m_commands;
        Key m_item;
        DS::ColorRgba m_color;
        uint8_t m_layer, m_delta;
        float m_weight;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        ClothingMsg(uint16_t type)
            : Message(type), m_commands(), m_layer(), m_delta(), m_weight() { }
    };
}

#endif
