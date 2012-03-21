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

#ifndef _MOUL_MESSAGE_H
#define _MOUL_MESSAGE_H

#include "creatable.h"
#include "Key.h"
#include <vector>

namespace MOUL
{
    class Message : public Creatable
    {
    public:
        enum BCastFlags
        {
            e_BCastByType               = (1<<0),
            e_Unused                    = (1<<1),
            e_PropagateToChildren       = (1<<2),
            e_ByExactType               = (1<<3),
            e_PropagateToModifiers      = (1<<4),
            e_ClearAfterBcast           = (1<<5),
            e_NetPropagate              = (1<<6),
            e_NetSent                   = (1<<7),
            e_NetUseRelevanceRegions    = (1<<8),
            e_NetForce                  = (1<<9),
            e_NetNonLocal               = (1<<10),
            e_LocalPropagate            = (1<<11),
            e_MsgWatch                  = (1<<12),
            e_NetStartCascade           = (1<<13),
            e_NetAllowInterAge          = (1<<14),
            e_NetSendUnreliable         = (1<<15),
            e_CCRSendToAllPlayers       = (1<<16),
            e_NetCreatedRemotely        = (1<<17),
        };

        Key m_sender;
        std::vector<Key> m_receivers;
        double m_timestamp;
        uint32_t m_bcastFlags;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream);

        virtual bool makeSafeForNet() { return true; }

    protected:
        Message(uint16_t type)
            : Creatable(type), m_timestamp(0.0), m_bcastFlags(0) { }
    };
}

#endif
