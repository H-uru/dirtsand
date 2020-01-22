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

#include "AvTask.h"
#include "factory.h"

void MOUL::AvAnimTask::read(DS::Stream* stream)
{
    m_animName = stream->readSafeString();
    m_initialBlend = stream->read<float>();
    m_targetBlend = stream->read<float>();
    m_fadeSpeed = stream->read<float>();
    m_setTime = stream->read<float>();
    m_start = stream->read<bool>();
    m_loop = stream->read<bool>();
    m_attach = stream->read<bool>();
}

void MOUL::AvAnimTask::write(DS::Stream* stream) const
{
    stream->writeSafeString(m_animName);
    stream->write<float>(m_initialBlend);
    stream->write<float>(m_targetBlend);
    stream->write<float>(m_fadeSpeed);
    stream->write<float>(m_setTime);
    stream->write<bool>(m_start);
    stream->write<bool>(m_loop);
    stream->write<bool>(m_attach);
}

void MOUL::AvOneShotLinkTask::read(DS::Stream* stream)
{
    m_animName = stream->readSafeString();
    m_markerName = stream->readSafeString();
}

void MOUL::AvOneShotLinkTask::write(DS::Stream* stream) const
{
    stream->writeSafeString(m_animName);
    stream->writeSafeString(m_markerName);
}

void MOUL::AvTaskBrain::read(DS::Stream* stream)
{
    Creatable::SafeUnref(m_brain);
    m_brain = Factory::Read<ArmatureBrain>(stream);
}

void MOUL::AvTaskBrain::write(DS::Stream* stream) const
{
    Factory::WriteCreatable(stream, m_brain);
}
