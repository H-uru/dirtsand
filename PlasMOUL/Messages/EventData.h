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

#ifndef _MOUL_EVENTDATA_H
#define _MOUL_EVENTDATA_H

#include "Key.h"
#include "Types/Math.h"
#include "streams.h"

namespace MOUL
{
    enum EventType
    {
        e_EvtInvalid, e_EvtCollision, e_EvtPicked, e_EvtControlKey,
        e_EvtVariable, e_EvtFacing, e_EvtContained, e_EvtActivate,
        e_EvtCallback, e_EvtResponderState, e_EvtMultiStage, e_EvtSpawned,
        e_EvtClickDrag, e_EvtCoop, e_EvtOfferLinkBook, e_EvtBook,
        e_EvtClimbingBlockerHit, e_EvtNone
    };

    enum EventDataType {
        e_DataFloat = 1, e_DataKey, e_DataInt, e_DataNull, e_DataNone
    };

    class EventData
    {
    public:
        int m_type;

        static EventData* Read(DS::Stream* stream);
        static void Write(DS::Stream* stream, EventData* data);

        virtual ~EventData() { }

    protected:
        EventData(int type) : m_type(type) { }

        virtual void read(DS::Stream* stream) { }
        virtual void write(DS::Stream* stream) const { }
    };

    class CollisionEventData : public EventData
    {
    public:
        CollisionEventData() : EventData(e_EvtCollision), m_enter() { }

        bool m_enter;
        Key m_hitter, m_hittee;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class PickedEventData : public EventData
    {
    public:
        PickedEventData() : EventData(e_EvtPicked), m_enabled() { }

        bool m_enabled;
        Key m_picker, m_picked;
        DS::Vector3 m_hitPoint;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class ControlKeyEventData : public EventData
    {
    public:
        ControlKeyEventData()
            : EventData(e_EvtControlKey), m_controlKey(), m_down() { }

        int32_t m_controlKey;
        bool m_down;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class VariableEventData : public EventData
    {
    public:
        VariableEventData()
            : EventData(e_EvtVariable), m_dataType(e_DataNone), m_number() { }

        int m_dataType;
        ST::string m_name;
        union {
            float f;
            uint32_t i;
        } m_number;
        Key m_key;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class FacingEventData : public EventData
    {
    public:
        FacingEventData() : EventData(e_EvtFacing), m_dot(), m_enabled() { }

        Key m_facer, m_facee;
        float m_dot;
        bool m_enabled;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class ContainedEventData : public EventData
    {
    public:
        ContainedEventData() : EventData(e_EvtContained), m_entering() { }

        bool m_entering;
        Key m_contained, m_container;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class ActivateEventData : public EventData
    {
    public:
        ActivateEventData()
            : EventData(e_EvtActivate), m_active(), m_activate() { }

        bool m_active, m_activate;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class CallbackEventData : public EventData
    {
    public:
        CallbackEventData() : EventData(e_EvtCallback), m_callbackType() { }

        int32_t m_callbackType;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class ResponderStateEventData : public EventData
    {
    public:
        ResponderStateEventData() : EventData(e_EvtResponderState), m_state() { }

        int32_t m_state;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class MultiStageEventData : public EventData
    {
    public:
        MultiStageEventData()
            : EventData(e_EvtMultiStage), m_stage(), m_event() { }

        int32_t m_stage, m_event;
        Key m_avatar;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class SpawnedEventData : public EventData
    {
    public:
        SpawnedEventData() : EventData(e_EvtSpawned) { }

        Key m_spawner, m_spanee;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class ClickDragEventData : public EventData
    {
    public:
        ClickDragEventData() : EventData(e_EvtClickDrag) { }
    };

    class CoopEventData : public EventData
    {
    public:
        CoopEventData() : EventData(e_EvtCoop), m_id(), m_serial() { }

        uint32_t m_id;
        uint16_t m_serial;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class OfferLinkBookEventData : public EventData
    {
    public:
        OfferLinkBookEventData()
            : EventData(e_EvtOfferLinkBook), m_targetAge(), m_offeree() { }

        Key m_offerer;
        uint32_t m_targetAge, m_offeree;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class BookEventData : public EventData
    {
    public:
        BookEventData() : EventData(e_EvtBook), m_event(), m_linkId() { }

        uint32_t m_event, m_linkId;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };

    class ClimbingBlockerHitEventData : public EventData
    {
    public:
        ClimbingBlockerHitEventData() : EventData(e_EvtClimbingBlockerHit) { }

        Key m_blocker;

    protected:
        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;
    };
}

#endif
