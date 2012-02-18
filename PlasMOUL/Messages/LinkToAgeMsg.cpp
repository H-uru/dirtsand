/******************************************************************************
 * This file is part of dirtsand.                                             *
 *                                                                            *
 * dirtsand is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation, either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * dirtsand is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.          *
 ******************************************************************************/

#include "LinkToAgeMsg.h"
#include "errors.h"

enum {
    kLinkToAgeVersion = 0
};

void MOUL::LinkToAgeMsg::read(DS::Stream* s)
{
    Message::read(s);

    DS_PASSERT(s->read<uint8_t>() == kLinkToAgeVersion);
    m_ageLink->read(s);
    m_linkInAnim = s->readSafeString();
}

void MOUL::LinkToAgeMsg::write(DS::Stream* s)
{
    Message::write(s);

    s->write<uint8_t>(kLinkToAgeVersion);
    m_ageLink->write(s);
    s->writeSafeString(m_linkInAnim);
}

bool MOUL::LinkToAgeMsg::makeSafeForNet()
{
    // The only time this msg should ever come over the wire is
    // as a field inside an AvBrainCoop's CoopCoordinator. So, if we get
    // here, then this is obviously a hack.
    // Except Cyan sucks and uses this directly in book sharing. We pass
    // it along for now.
    return true;
}
