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

#include "PlayerPage.h"

void MOUL::NetMsgPlayerPage::read(DS::Stream* stream)
{
    MOUL::NetMessage::read(stream);
    m_unload = stream->read<uint8_t>();
    m_uoid.read(stream);
}

void MOUL::NetMsgPlayerPage::write(DS::Stream* stream)
{
    MOUL::NetMessage::write(stream);
    stream->write<uint8_t>(m_unload);
    m_uoid.write(stream);
}
