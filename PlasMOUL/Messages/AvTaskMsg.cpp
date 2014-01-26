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

#include "AvTaskMsg.h"
#include "factory.h"

void MOUL::AvTaskMsg::read(DS::Stream* stream)
{
    Message::read(stream);

    if (stream->read<bool>())
        m_task = Factory::Read<AvTask>(stream);
    else
        m_task = nullptr;
}

void MOUL::AvTaskMsg::write(DS::Stream* stream) const
{
    Message::write(stream);

    if (m_task) {
        stream->write<bool>(true);
        Factory::WriteCreatable(stream, m_task);
    } else {
        stream->write<bool>(false);
    }
}

void MOUL::AvPushBrainMsg::read(DS::Stream* stream)
{
    AvTaskMsg::read(stream);
    m_brain = Factory::Read<ArmatureBrain>(stream);
}

void MOUL::AvPushBrainMsg::write(DS::Stream* stream) const
{
    AvTaskMsg::write(stream);
    Factory::WriteCreatable(stream, m_brain);
}
