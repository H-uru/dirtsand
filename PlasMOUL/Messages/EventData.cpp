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

#include "EventData.h"
#include "errors.h"

MOUL::EventData* MOUL::EventData::Read(DS::Stream* stream)
{
    EventData* data;
    const EventType type = stream->read<EventType, uint32_t>();
    switch (type) {
    case e_EvtCollision:
        data = new CollisionEventData();
        break;
    case e_EvtPicked:
        data = new PickedEventData();
        break;
    case e_EvtControlKey:
        data = new ControlKeyEventData();
        break;
    case e_EvtVariable:
        data = new VariableEventData();
        break;
    case e_EvtFacing:
        data = new FacingEventData();
        break;
    case e_EvtContained:
        data = new ContainedEventData();
        break;
    case e_EvtActivate:
        data = new ActivateEventData();
        break;
    case e_EvtCallback:
        data = new CallbackEventData();
        break;
    case e_EvtResponderState:
        data = new ResponderStateEventData();
        break;
    case e_EvtMultiStage:
        data = new MultiStageEventData();
        break;
    case e_EvtSpawned:
        data = new SpawnedEventData();
        break;
    case e_EvtClickDrag:
        data = new ClickDragEventData();
        break;
    case e_EvtCoop:
        data = new CoopEventData();
        break;
    case e_EvtOfferLinkBook:
        data = new OfferLinkBookEventData();
        break;
    case e_EvtBook:
        data = new BookEventData();
        break;
    case e_EvtClimbingBlockerHit:
        data = new ClimbingBlockerHitEventData();
        break;
    default:
        ST::printf(stderr, "Got unsupported EventData type {}\n", type);
        throw DS::MalformedData();
    }

    try {
        data->read(stream);
    } catch (...) {
        delete data;
        throw;
    }
    return data;
}

void MOUL::EventData::Write(DS::Stream* stream, MOUL::EventData* data)
{
    stream->write<EventType, uint32_t>(data->m_type);
    data->write(stream);
}

void MOUL::CollisionEventData::read(DS::Stream* stream)
{
    m_enter = stream->read<bool>();
    m_hitter.read(stream);
    m_hittee.read(stream);
}

void MOUL::CollisionEventData::write(DS::Stream* stream) const
{
    stream->write<bool>(m_enter);
    m_hitter.write(stream);
    m_hittee.write(stream);
}

void MOUL::PickedEventData::read(DS::Stream* stream)
{
    m_picker.read(stream);
    m_picked.read(stream);
    m_enabled = stream->read<bool>();
    m_hitPoint = stream->read<DS::Vector3>();
}

void MOUL::PickedEventData::write(DS::Stream* stream) const
{
    m_picker.write(stream);
    m_picked.write(stream);
    stream->write<bool>(m_enabled);
    stream->write<DS::Vector3>(m_hitPoint);
}

void MOUL::ControlKeyEventData::read(DS::Stream* stream)
{
    m_controlKey = stream->read<int32_t>();
    m_down = stream->read<bool>();
}

void MOUL::ControlKeyEventData::write(DS::Stream* stream) const
{
    stream->write<int32_t>(m_controlKey);
    stream->write<bool>(m_down);
}

void MOUL::VariableEventData::read(DS::Stream* stream)
{
    m_name = stream->readSafeString();
    m_dataType = stream->read<EventDataType, uint32_t>();
    switch(m_dataType) {
    case e_DataFloat:
        m_number.f = stream->read<float>();
        break;
    case e_DataInt:
        m_number.i = stream->read<uint32_t>();
        break;
    default:
        stream->read<uint32_t>();
    }
    m_key.read(stream);
}

void MOUL::VariableEventData::write(DS::Stream* stream) const
{
    stream->writeSafeString(m_name);
    stream->write<EventDataType, uint32_t>(m_dataType);
    switch(m_dataType) {
    case e_DataFloat:
        stream->write<float>(m_number.f);
        break;
    case e_DataInt:
        stream->write<uint32_t>(m_number.i);
        break;
    default:
        stream->write<uint32_t>(0);
    }
    m_key.write(stream);
}

void MOUL::FacingEventData::read(DS::Stream* stream)
{
    m_facer.read(stream);
    m_facee.read(stream);
    m_dot = stream->read<float>();
    m_enabled = stream->read<bool>();
}

void MOUL::FacingEventData::write(DS::Stream* stream) const
{
    m_facer.write(stream);
    m_facee.write(stream);
    stream->write<float>(m_dot);
    stream->write<bool>(m_enabled);
}

void MOUL::ContainedEventData::read(DS::Stream* stream)
{
    m_contained.read(stream);
    m_container.read(stream);
    m_entering = stream->read<bool>();
}

void MOUL::ContainedEventData::write(DS::Stream* stream) const
{
    m_contained.write(stream);
    m_container.write(stream);
    stream->write<bool>(m_entering);
}

void MOUL::ActivateEventData::read(DS::Stream* stream)
{
    m_active = stream->read<bool>();
    m_activate = stream->read<bool>();
}

void MOUL::ActivateEventData::write(DS::Stream* stream) const
{
    stream->write<bool>(m_active);
    stream->write<bool>(m_activate);
}

void MOUL::CallbackEventData::read(DS::Stream* stream)
{
    m_callbackType = stream->read<int32_t>();
}

void MOUL::CallbackEventData::write(DS::Stream* stream) const
{
    stream->write<int32_t>(m_callbackType);
}

void MOUL::ResponderStateEventData::read(DS::Stream* stream)
{
    m_state = stream->read<int32_t>();
}

void MOUL::ResponderStateEventData::write(DS::Stream* stream) const
{
    stream->write<int32_t>(m_state);
}

void MOUL::MultiStageEventData::read(DS::Stream* stream)
{
    m_stage = stream->read<int32_t>();
    m_event = stream->read<int32_t>();
    m_avatar.read(stream);
}

void MOUL::MultiStageEventData::write(DS::Stream* stream) const
{
    stream->write<int32_t>(m_stage);
    stream->write<int32_t>(m_event);
    m_avatar.write(stream);
}

void MOUL::SpawnedEventData::read(DS::Stream* stream)
{
    m_spawner.read(stream);
    m_spanee.read(stream);
}

void MOUL::SpawnedEventData::write(DS::Stream* stream) const
{
    m_spawner.write(stream);
    m_spanee.write(stream);
}

void MOUL::CoopEventData::read(DS::Stream* stream)
{
    m_id = stream->read<uint32_t>();
    m_serial = stream->read<uint16_t>();
}

void MOUL::CoopEventData::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_id);
    stream->write<uint16_t>(m_serial);
}

void MOUL::OfferLinkBookEventData::read(DS::Stream* stream)
{
    m_offerer.read(stream);
    m_targetAge = stream->read<uint32_t>();
    m_offeree = stream->read<uint32_t>();
}

void MOUL::OfferLinkBookEventData::write(DS::Stream* stream) const
{
    m_offerer.write(stream);
    stream->write<uint32_t>(m_targetAge);
    stream->write<uint32_t>(m_offeree);
}

void MOUL::BookEventData::read(DS::Stream* stream)
{
    m_event = stream->read<uint32_t>();
    m_linkId = stream->read<uint32_t>();
}

void MOUL::BookEventData::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_event);
    stream->write<uint32_t>(m_linkId);
}

void MOUL::ClimbingBlockerHitEventData::read(DS::Stream* stream)
{
    m_blocker.read(stream);
}

void MOUL::ClimbingBlockerHitEventData::write(DS::Stream* stream) const
{
    m_blocker.write(stream);
}
