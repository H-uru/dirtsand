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

#include "NetMsgGameState.h"

void MOUL::NetMsgInitialAgeStateSent::read(DS::Stream* stream)
{
    NetMessage::read(stream);
    m_numStates = stream->read<uint32_t>();
}

void MOUL::NetMsgInitialAgeStateSent::write(DS::Stream* stream) const
{
    NetMessage::write(stream);
    stream->write<uint32_t>(m_numStates);
}
