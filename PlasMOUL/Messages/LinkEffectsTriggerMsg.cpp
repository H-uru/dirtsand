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

#include "LinkEffectsTriggerMsg.h"

void MOUL::LinkEffectsTriggerMsg::read(DS::Stream* stream)
{
    Message::read(stream);

    m_invisLevel = stream->read<uint32_t>();
    m_leaving = stream->read<bool>();
    m_linkKey.read(stream);
    m_effects = stream->read<uint32_t>();
    m_animKey.read(stream);
}

void MOUL::LinkEffectsTriggerMsg::write(DS::Stream* stream)
{
    Message::write(stream);

    stream->write<uint32_t>(m_invisLevel);
    stream->write<bool>(m_leaving);
    m_linkKey.write(stream);
    stream->write<uint32_t>(m_effects);
    m_animKey.write(stream);
}
