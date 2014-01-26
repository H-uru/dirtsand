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

#include "LinkingMgrMsg.h"

enum Flags
{
    e_Command,
    e_Args,
};

void MOUL::LinkingMgrMsg::read(DS::Stream* stream)
{
    Message::read(stream);
    m_contentFlags.read(stream);
    if (m_contentFlags.get(e_Command))
        m_cmd = stream->read<uint8_t>();
    if (m_contentFlags.get(e_Args))
        m_args.read(stream);
}

void MOUL::LinkingMgrMsg::write(DS::Stream* stream) const
{
    Message::write(stream);
    m_contentFlags.write(stream);
    if (m_contentFlags.get(e_Command))
        stream->write<uint8_t>(m_cmd);
    if (m_contentFlags.get(e_Args))
        m_args.write(stream);
}
