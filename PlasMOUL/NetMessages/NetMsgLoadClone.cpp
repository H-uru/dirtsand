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

#include "NetMsgLoadClone.h"

void MOUL::NetMsgLoadClone::read(DS::Stream* stream)
{
    NetMsgGameMessage::read(stream);

    m_object.read(stream);
    m_isPlayer = stream->read<bool>();
    m_isLoading = stream->read<bool>();
    m_isInitialState = stream->read<bool>();
}

void MOUL::NetMsgLoadClone::write(DS::Stream* stream)
{
    NetMsgGameMessage::write(stream);

    m_object.write(stream);
    stream->write<bool>(m_isPlayer);
    stream->write<bool>(m_isLoading);
    stream->write<bool>(m_isInitialState);
}
