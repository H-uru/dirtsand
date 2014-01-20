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

#include "AvSeekMsg.h"

void MOUL::AvSeekMsg::read(DS::Stream* stream)
{
    AvTaskMsg::read(stream);

    m_seekPoint.read(stream);
    if (m_seekPoint.isNull()) {
        m_targetPos = stream->read<DS::Vector3>();
        m_targetLook = stream->read<DS::Vector3>();
    }
    m_duration = stream->read<float>();
    m_smartSeek = stream->read<bool>();
    m_animName = stream->readSafeString();
    m_alignType = stream->read<uint16_t>();
    m_noSeek = stream->read<bool>();
    m_flags = stream->read<uint8_t>();
    m_finishKey.read(stream);
}

void MOUL::AvSeekMsg::write(DS::Stream* stream) const
{
    AvTaskMsg::write(stream);

    m_seekPoint.write(stream);
    if (m_seekPoint.isNull()) {
        stream->write<DS::Vector3>(m_targetPos);
        stream->write<DS::Vector3>(m_targetLook);
    }
    stream->write<float>(m_duration);
    stream->write<bool>(m_smartSeek);
    stream->writeSafeString(m_animName);
    stream->write<uint16_t>(m_alignType);
    stream->write<bool>(m_noSeek);
    stream->write<uint8_t>(m_flags);
    m_finishKey.write(stream);
}

void MOUL::AvOneShotMsg::read(DS::Stream* stream)
{
    AvSeekMsg::read(stream);

    m_oneShotAnimName = stream->readSafeString();
    m_drivable = stream->read<bool>();
    m_reversible = stream->read<bool>();
}

void MOUL::AvOneShotMsg::write(DS::Stream* stream) const
{
    AvSeekMsg::write(stream);

    stream->writeSafeString(m_oneShotAnimName);
    stream->write<bool>(m_drivable);
    stream->write<bool>(m_reversible);
}
