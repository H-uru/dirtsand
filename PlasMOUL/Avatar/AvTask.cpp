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

#include "AvTask.h"

void MOUL::AvAnimTask::read(DS::Stream* stream)
{
    m_animName = stream->readSafeString();
    m_initialBlend = stream->read<float>();
    m_targetBlend = stream->read<float>();
    m_fadeSpeed = stream->read<float>();
    m_setTime = stream->read<float>();
    m_start = stream->readBool();
    m_loop = stream->readBool();
    m_attach = stream->readBool();
}

void MOUL::AvAnimTask::write(DS::Stream* stream)
{
    stream->writeSafeString(m_animName);
    stream->write<float>(m_initialBlend);
    stream->write<float>(m_targetBlend);
    stream->write<float>(m_fadeSpeed);
    stream->write<float>(m_setTime);
    stream->writeBool(m_start);
    stream->writeBool(m_loop);
    stream->writeBool(m_attach);
}

void MOUL::AvOneShotLinkTask::read(DS::Stream* stream)
{
    m_animName = stream->readSafeString();
    m_markerName = stream->readSafeString();
}

void MOUL::AvOneShotLinkTask::write(DS::Stream* stream)
{
    stream->writeSafeString(m_animName);
    stream->writeSafeString(m_markerName);
}

#include "errors.h"
void MOUL::AvTaskBrain::read(DS::Stream* stream)
{
    //TODO
    DS_PASSERT(0);
}

void MOUL::AvTaskBrain::write(DS::Stream* stream)
{
    DS_PASSERT(0);
}
