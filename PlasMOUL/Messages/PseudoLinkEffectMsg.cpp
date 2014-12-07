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

#include "PseudoLinkEffectMsg.h"

void MOUL::PseudoLinkEffectMsg::read(DS::Stream* s)
{
    Message::read(s);

    m_linkObj.read(s);
    m_avatar.read(s);
}

void MOUL::PseudoLinkEffectMsg::write(DS::Stream* s) const
{
    Message::write(s);

    m_linkObj.write(s);
    m_avatar.write(s);
}
