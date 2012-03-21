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

#include "CoopCoordinator.h"
#include "AvBrainCoop.h"
#include "factory.h"
#include "Messages/Message.h"

MOUL::CoopCoordinator::~CoopCoordinator()
{
    m_hostBrain->unref();
    m_guestBrain->unref();
    m_acceptMsg->unref();
}

void MOUL::CoopCoordinator::read(DS::Stream* s)
{
    m_hostKey.read(s);
    m_guestKey.read(s);

    m_hostBrain->unref();
    m_guestBrain->unref();
    m_hostBrain = Factory::Read<AvBrainCoop>(s);
    m_guestBrain = Factory::Read<AvBrainCoop>(s);

    m_hostOfferStage = s->read<uint8_t>();
    m_guestAcceptStage = s->read<bool>();

    m_acceptMsg->unref();
    if (s->read<bool>())
        m_acceptMsg = Factory::Read<Message>(s);
    else
        m_acceptMsg = 0;
    m_synchBone = s->readSafeString();
    m_autoStartGuest = s->read<bool>();
}

void MOUL::CoopCoordinator::write(DS::Stream* s)
{
    m_hostKey.write(s);
    m_guestKey.write(s);

    Factory::WriteCreatable(s, m_hostBrain);
    Factory::WriteCreatable(s, m_guestBrain);

    s->write<uint8_t>(m_hostOfferStage);
    s->write<bool>(m_guestAcceptStage);

    s->write<bool>(m_acceptMsg);
    if (m_acceptMsg)
        Factory::WriteCreatable(s, m_acceptMsg);
    s->writeSafeString(m_synchBone);
    s->write<bool>(m_autoStartGuest);
}
