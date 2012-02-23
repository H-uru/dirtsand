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

#include "MultistageMsg.h"

void MOUL::MultistageModMsg::read(DS::Stream* s)
{
    MOUL::Message::read(s);

    m_cmds.read(s);
    m_stage = s->read<uint8_t>();
    m_numLoops = s->read<uint8_t>();
}

void MOUL::MultistageModMsg::write(DS::Stream* s)
{
    MOUL::Message::write(s);

    m_cmds.write(s);
    s->write<uint8_t>(m_stage);
    s->write<uint8_t>(m_numLoops);
}
