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

#include "NetMsgSharedState.h"
#include "errors.h"

void MOUL::GenericType::read(DS::Stream* stream)
{
    m_type = stream->read<uint8_t>();

    switch (m_type) {
    case e_TypeInt:
    case e_TypeUint:
        m_uint = stream->read<uint32_t>();
        break;
    case e_TypeString:
    case e_TypeAny:
        m_string = stream->readSafeString(DS::e_StringUTF8);
        break;
    case e_TypeFloat:
        m_float = stream->read<float>();
        break;
    case e_TypeBool:
        m_bool = stream->read<bool>();
        break;
    case e_TypeByte:
        m_byte = stream->read<uint8_t>();
        break;
    case e_TypeDouble:
        m_double = stream->read<double>();
        break;
    }
}

void MOUL::GenericType::write(DS::Stream* stream) const
{
    stream->write<uint8_t>(m_type);

    switch (m_type) {
    case e_TypeInt:
    case e_TypeUint:
        stream->write<uint32_t>(m_uint);
        break;
    case e_TypeString:
    case e_TypeAny:
        stream->writeSafeString(m_string, DS::e_StringUTF8);
        break;
    case e_TypeFloat:
        stream->write<float>(m_float);
        break;
    case e_TypeBool:
        stream->write<bool>(m_bool);
        break;
    case e_TypeByte:
        stream->write<uint8_t>(m_byte);
        break;
    case e_TypeDouble:
        stream->write<double>(m_double);
        break;
    }
}

void MOUL::GenericVar::read(DS::Stream* stream)
{
    m_name = stream->readSafeString(DS::e_StringUTF8);
    m_value.read(stream);
}

void MOUL::GenericVar::write(DS::Stream* stream) const
{
    stream->writeSafeString(m_name, DS::e_StringUTF8);
    m_value.write(stream);
}

void MOUL::NetMsgSharedState::read(DS::Stream* stream)
{
    NetMsgObject::read(stream);

    NetMsgStream msgStream;
    msgStream.read(stream);
    m_compression = msgStream.m_compression;

    // Read the state
    m_stateName = msgStream.m_stream.readPString<uint16_t>(DS::e_StringUTF8);
    m_vars.resize(msgStream.m_stream.read<uint32_t>());
    m_serverMayDelete = msgStream.m_stream.read<bool>();

    for (size_t i=0; i<m_vars.size(); ++i)
        m_vars[i].read(&msgStream.m_stream);
    if (!msgStream.m_stream.atEof()) {
        ST::printf(stderr, "WARNING: {} bytes left over in stream after parsing "
                           "NetMsgSharedState state variables\n",
                   msgStream.m_stream.size() - msgStream.m_stream.tell());
    }

    m_lockRequest = stream->read<uint8_t>();
}

void MOUL::NetMsgSharedState::write(DS::Stream* stream) const
{
    NetMsgObject::write(stream);

    // Save the state to a stream
    NetMsgStream msgStream;
    msgStream.m_compression = m_compression;

    msgStream.m_stream.writePString<uint16_t>(m_stateName, DS::e_StringUTF8);
    msgStream.m_stream.write<uint32_t>(m_vars.size());
    msgStream.m_stream.write<bool>(m_serverMayDelete);

    for (size_t i=0; i<m_vars.size(); ++i)
        m_vars[i].write(&msgStream.m_stream);
    msgStream.m_stream.seek(0, SEEK_SET);
    msgStream.write(stream);

    stream->write<uint8_t>(m_lockRequest);
}
