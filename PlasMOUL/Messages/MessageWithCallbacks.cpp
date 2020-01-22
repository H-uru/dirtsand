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

#include "MessageWithCallbacks.h"
#include "PlasMOUL/factory.h"

MOUL::MessageWithCallbacks::~MessageWithCallbacks()
{
    for (Message* callback : m_callbacks)
        Creatable::SafeUnref(callback);
}

void MOUL::MessageWithCallbacks::read(DS::Stream* stream)
{
    Message::read(stream);

    for (Message* callback : m_callbacks)
        Creatable::SafeUnref(callback);

    m_callbacks.resize(stream->read<uint32_t>());
    for (size_t i = 0; i < m_callbacks.capacity(); ++i)
        m_callbacks[i] = MOUL::Factory::Read<MOUL::Message>(stream);
}

void MOUL::MessageWithCallbacks::write(DS::Stream* stream) const
{
    Message::write(stream);

    stream->write<uint32_t>(m_callbacks.size());
    for (Message* callback : m_callbacks)
        MOUL::Factory::WriteCreatable(stream, callback);
}

bool MOUL::MessageWithCallbacks::makeSafeForNet()
{
    for (Message* callback : m_callbacks) {
        if (!callback->makeSafeForNet())
            return false;
    }
    return true;
}

void MOUL::AnimCmdMsg::read(DS::Stream* stream)
{
    MessageWithCallbacks::read(stream);

    m_cmd.read(stream);
    m_begin = stream->read<float>();
    m_end = stream->read<float>();
    m_loopEnd = stream->read<float>();
    m_loopBegin = stream->read<float>();
    m_speed = stream->read<float>();
    m_speedChangeRate = stream->read<float>();
    m_time = stream->read<float>();
    m_animName = stream->readSafeString();
    m_loopName = stream->readSafeString();
}

void MOUL::AnimCmdMsg::write(DS::Stream* stream) const
{
    MessageWithCallbacks::write(stream);

    m_cmd.write(stream);
    stream->write<float>(m_begin);
    stream->write<float>(m_end);
    stream->write<float>(m_loopEnd);
    stream->write<float>(m_loopBegin);
    stream->write<float>(m_speed);
    stream->write<float>(m_speedChangeRate);
    stream->write<float>(m_time);
    stream->writeSafeString(m_animName);
    stream->writeSafeString(m_loopName);
}
