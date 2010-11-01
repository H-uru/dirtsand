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

#include "KIMessage.h"

void MOUL::KIMessage::read(DS::Stream* stream)
{
    Message::read(stream);
}

void MOUL::KIMessage::write(DS::Stream* stream)
{
    Message::write(stream);
}

bool MOUL::KIMessage::makeSafeForNet()
{
    if (m_command != e_ChatMessage) {
        // Client is being naughty
        return false;
    }
    m_flags &= ~e_AdminMsg;
    return true;
}
