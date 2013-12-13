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

#include "ArmatureBrain.h"
#include "Key.h"

void MOUL::ArmatureBrain::read(DS::Stream* stream)
{
    // Pointless waste of bytes
    stream->read<uint32_t>();
    if (stream->read<bool>()) {
        Key justPretendThisIsntHere;
        justPretendThisIsntHere.read(stream);
    }
    stream->read<uint32_t>();
    stream->read<float>();
    stream->read<double>();
}

void MOUL::ArmatureBrain::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(0);
    stream->write<bool>(false);
    stream->write<uint32_t>(0);
    stream->write<float>(0);
    stream->write<double>(0);
}

void MOUL::AvBrainHuman::read(DS::Stream* stream)
{
    ArmatureBrain::read(stream);
    m_isCustomAvatar = stream->read<bool>();
}

void MOUL::AvBrainHuman::write(DS::Stream* stream) const
{
    ArmatureBrain::write(stream);
    stream->write<bool>(m_isCustomAvatar);
}
