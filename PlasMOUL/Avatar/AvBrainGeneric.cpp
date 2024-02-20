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

#include "AvBrainGeneric.h"
#include "factory.h"

MOUL::AvBrainGeneric::~AvBrainGeneric()
{
    for (auto it = m_stages.begin(); it != m_stages.end(); ++it)
        Creatable::SafeUnref(*it);
    Creatable::SafeUnref(m_startMessage);
    Creatable::SafeUnref(m_endMessage);
}

void MOUL::AvBrainGeneric::read(DS::Stream* stream)
{
    MOUL::ArmatureBrain::read(stream);

    for (auto it = m_stages.begin(); it != m_stages.end(); ++it)
        Creatable::SafeUnref((*it));

    m_stages.resize(stream->read<uint32_t>());
    for (size_t i = 0; i < m_stages.size(); ++i) {
        m_stages[i] = Factory::Read<AnimStage>(stream);
        m_stages[i]->readAux(stream);
    }
    m_curStage = stream->read<int32_t>();

    m_type = stream->read<uint32_t>();
    m_exitFlags = stream->read<uint32_t>();
    m_mode = stream->read<uint8_t>();
    m_forward = stream->read<bool>();

    Creatable::SafeUnref(m_startMessage);
    if (stream->read<bool>())
        m_startMessage = Factory::Read<Message>(stream);
    else
        m_startMessage = nullptr;

    Creatable::SafeUnref(m_endMessage);
    if (stream->read<bool>())
        m_endMessage = Factory::Read<Message>(stream);
    else
        m_endMessage = nullptr;

    m_fadeIn = stream->read<float>();
    m_fadeOut = stream->read<float>();
    m_moveMode = stream->read<uint8_t>();
    m_bodyUsage = stream->read<uint8_t>();
    m_recipient.read(stream);
}

void MOUL::AvBrainGeneric::write(DS::Stream* stream) const
{
    MOUL::ArmatureBrain::write(stream);

    stream->write<uint32_t>(m_stages.size());
    for (size_t i = 0; i < m_stages.size(); ++i) {
        Factory::WriteCreatable(stream, m_stages[i]);
        m_stages[i]->writeAux(stream);
    }
    stream->write<int32_t>(m_curStage);

    stream->write<uint32_t>(m_type);
    stream->write<uint32_t>(m_exitFlags);
    stream->write<uint8_t>(m_mode);
    stream->write<bool>(m_forward);

    Factory::WriteCreatable(stream, m_startMessage);
    Factory::WriteCreatable(stream, m_endMessage);

    stream->write<float>(m_fadeIn);
    stream->write<float>(m_fadeOut);
    stream->write<uint8_t>(m_moveMode);
    stream->write<uint8_t>(m_bodyUsage);
    m_recipient.write(stream);
}

bool MOUL::AvBrainGeneric::makeSafeForNet()
{
    // These fields can contain arbitrary messages,
    // so we must check them before sending the brain to other clients.
    // The client never uses these fields for brains sent over the network though,
    // so for simplicity, don't allow them over the network at all.
    // If this causes issues, we could specifically allow NotifyMsg
    // (the only message type that the client puts in these fields)
    // or recursively call makeSafeForNet() on the messages.
    return m_startMessage == nullptr && m_endMessage == nullptr;
}
