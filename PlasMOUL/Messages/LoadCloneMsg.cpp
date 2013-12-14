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

#include "LoadCloneMsg.h"
#include "factory.h"

void MOUL::LoadCloneMsg::read(DS::Stream* stream)
{
    Message::read(stream);

    m_cloneKey.read(stream);
    m_requestorKey.read(stream);
    m_originPlayerId = stream->read<uint32_t>();
    m_userData = stream->read<uint32_t>();
    m_validMsg = stream->read<bool>();
    m_isLoading = stream->read<bool>();
    m_triggerMsg->unref();
    m_triggerMsg = Factory::Read<Message>(stream);
}

void MOUL::LoadCloneMsg::write(DS::Stream* stream) const
{
    MOUL::Message::write(stream);

    m_cloneKey.write(stream);
    m_requestorKey.write(stream);
    stream->write<uint32_t>(m_originPlayerId);
    stream->write<uint32_t>(m_userData);
    stream->write<bool>(m_validMsg);
    stream->write<bool>(m_isLoading);
    Factory::WriteCreatable(stream, m_triggerMsg);
}
