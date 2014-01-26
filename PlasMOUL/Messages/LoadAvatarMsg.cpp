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

#include "LoadAvatarMsg.h"
#include "factory.h"

void MOUL::LoadAvatarMsg::read(DS::Stream* stream)
{
    LoadCloneMsg::read(stream);

    m_isPlayer = stream->read<bool>();
    m_spawnPoint.read(stream);
    m_initTask->unref();
    if (stream->read<bool>())
        m_initTask = Factory::Read<AvTask>(stream);
    else
        m_initTask = nullptr;
    m_userString = stream->readSafeString();
}

void MOUL::LoadAvatarMsg::write(DS::Stream* stream) const
{
    LoadCloneMsg::write(stream);

    stream->write<bool>(m_isPlayer);
    m_spawnPoint.write(stream);
    if (m_initTask) {
        stream->write<bool>(true);
        Factory::WriteCreatable(stream, m_initTask);
    } else {
        stream->write<bool>(false);
    }
    stream->writeSafeString(m_userString);
}
