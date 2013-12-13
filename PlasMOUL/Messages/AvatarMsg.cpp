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

#include "AvatarMsg.h"
#include "Avatar/CoopCoordinator.h"
#include "factory.h"

void MOUL::AvBrainGenericMsg::read(DS::Stream* stream)
{
    Message::read(stream);

    m_type = static_cast<Type>(stream->read<uint32_t>());
    m_stage = stream->read<int32_t>();
    m_setTime = stream->read<bool>();
    m_newTime = stream->read<float>();
    m_setDirection = stream->read<bool>();
    m_newDirection = stream->read<bool>();
    m_transitionTime = stream->read<float>();
}

void MOUL::AvBrainGenericMsg::write(DS::Stream* stream) const
{
    Message::write(stream);

    stream->write<uint32_t>(m_type);
    stream->write<int32_t>(m_stage);
    stream->write<bool>(m_setTime);
    stream->write<float>(m_newTime);
    stream->write<bool>(m_setDirection);
    stream->write<bool>(m_newDirection);
    stream->write<float>(m_transitionTime);
}

void MOUL::AvCoopMsg::read(DS::Stream* s)
{
    Message::read(s);

    m_coordinator->unref();
    if (s->read<bool>())
        m_coordinator = Factory::Read<CoopCoordinator>(s);
    else
        m_coordinator = 0;
    m_initiatorId = s->read<uint32_t>();
    m_initiatorSerial = s->read<uint16_t>();
    m_command = (Command)s->read<uint16_t>();
}

void MOUL::AvCoopMsg::write(DS::Stream* s) const
{
    Message::write(s);

    s->write<bool>(m_coordinator);
    if (m_coordinator)
        Factory::WriteCreatable(s, m_coordinator);
    s->write<uint32_t>(m_initiatorId);
    s->write<uint16_t>(m_initiatorSerial);
    s->write<uint16_t>(m_command);
}

bool MOUL::AvCoopMsg::makeSafeForNet()
{
    if (m_coordinator)
        if (m_coordinator->m_acceptMsg)
            // This can only be LinkToAgeMsg.
            // Indeed, this will be the only way to send that msg over the wire
            return m_coordinator->m_acceptMsg->type() == ID_LinkToAgeMsg;
    return true;
}

void MOUL::AvTaskSeekDoneMsg::read(DS::Stream* stream)
{
    Message::read(stream);
    m_aborted = stream->read<bool>();
}

void MOUL::AvTaskSeekDoneMsg::write(DS::Stream* stream) const
{
    Message::write(stream);
    stream->write<bool>(m_aborted);
}
