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

#include "AnimStage.h"

void MOUL::AnimStage::read(DS::Stream* stream)
{
    m_animName = stream->readSafeString();
    m_notify = stream->read<uint8_t>();
    m_forwardType = stream->read<uint32_t>();
    m_backType = stream->read<uint32_t>();
    m_advanceType = stream->read<uint32_t>();
    m_regressType = stream->read<uint32_t>();
    m_loops = stream->read<uint32_t>();
    m_doAdvance = stream->read<bool>();
    m_advanceTo = stream->read<uint32_t>();
    m_doRegress = stream->read<bool>();
    m_regressTo = stream->read<uint32_t>();
}

void MOUL::AnimStage::write(DS::Stream* stream) const
{
    stream->writeSafeString(m_animName);
    stream->write<uint8_t>(m_notify);
    stream->write<uint32_t>(m_forwardType);
    stream->write<uint32_t>(m_backType);
    stream->write<uint32_t>(m_advanceType);
    stream->write<uint32_t>(m_regressType);
    stream->write<uint32_t>(m_loops);
    stream->write<bool>(m_doAdvance);
    stream->write<uint32_t>(m_advanceTo);
    stream->write<bool>(m_doRegress);
    stream->write<uint32_t>(m_regressTo);
}

void MOUL::AnimStage::readAux(DS::Stream* stream)
{
    m_localTime = stream->read<float>();
    m_length = stream->read<float>();
    m_curLoop = stream->read<int32_t>();
    m_attached = stream->read<bool>();
}

void MOUL::AnimStage::writeAux(DS::Stream* stream)
{
    stream->write<float>(m_localTime);
    stream->write<float>(m_length);
    stream->write<int32_t>(m_curLoop);
    stream->write<bool>(m_attached);
}
