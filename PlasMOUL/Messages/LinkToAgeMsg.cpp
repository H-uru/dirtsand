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

static uint8_t kLinkToAgeVersion = 0;

MOUL::LinkToAgeMsg::~LinkToAgeMsg()
{
    m_ageLink->unref();
}

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
