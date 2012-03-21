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

#include "ParticleMsg.h"

void MOUL::ParticleKillMsg::read(DS::Stream* s)
{
    Message::read(s);

    m_numToKill = s->read<float>();
    m_timeLeft = s->read<float>();
    m_flags = s->read<uint8_t>();
}

void MOUL::ParticleKillMsg::write(DS::Stream* s)
{
    Message::write(s);

    s->write<float>(m_numToKill);
    s->write<float>(m_timeLeft);
    s->write<uint8_t>(m_flags);
}

void MOUL::ParticleTransferMsg::read(DS::Stream* s)
{
    Message::read(s);

    m_sysKey.read(s);
    m_transferCount = s->read<uint16_t>();
}

void MOUL::ParticleTransferMsg::write(DS::Stream* s)
{
    Message::write(s);

    m_sysKey.write(s);
    s->write<uint16_t>(m_transferCount);
}
