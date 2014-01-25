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

#ifndef _MOUL_COOPCOORDINATOR
#define _MOUL_COOPCOORDINATOR

#include "creatable.h"
#include "Key.h"
#include "strings.h"

namespace MOUL
{
    class AvBrainCoop;
    class Message;

    // Technically a KeyedObject, but the key is never serialized, so
    // we can get away with this evil trick here
    class CoopCoordinator : public Creatable
    {
        FACTORY_CREATABLE(CoopCoordinator)

        virtual void read(DS::Stream* s);
        virtual void write(DS::Stream* s) const;

    public:
        Key m_hostKey, m_guestKey;
        AvBrainCoop* m_hostBrain;
        AvBrainCoop* m_guestBrain;
        uint8_t m_hostOfferStage;
        bool m_guestAcceptStage;
        Message* m_acceptMsg;
        DS::String m_synchBone;
        bool m_autoStartGuest;

    protected:
        CoopCoordinator(uint16_t type)
            : Creatable(type), m_hostBrain(), m_guestBrain(),
              m_hostOfferStage(), m_guestAcceptStage(), m_acceptMsg(),
              m_autoStartGuest() { }

        virtual ~CoopCoordinator();
    };
};

#endif // _MOUL_COOPCOORDINATOR
