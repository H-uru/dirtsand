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

#include "LoadAvatarMsg.h"
#include "factory.h"

void MOUL::LoadAvatarMsg::read(DS::Stream* stream)
{
    LoadCloneMsg::read(stream);

    m_isPlayer = stream->readBool();
    m_spawnPoint.read(stream);
    if (stream->readBool())
        m_initTask = Factory::Read<AvTask>(stream);
    m_userString = stream->readSafeString();
}

void MOUL::LoadAvatarMsg::write(DS::Stream* stream)
{
    LoadCloneMsg::write(stream);

    stream->writeBool(m_isPlayer);
    m_spawnPoint.write(stream);
    if (m_initTask) {
        stream->writeBool(true);
        Factory::WriteCreatable(stream, m_initTask);
    } else {
        stream->writeBool(false);
    }
    stream->writeSafeString(m_userString);
}
