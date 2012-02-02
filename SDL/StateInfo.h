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

#ifndef _SDL_STATEINFO_H
#define _SDL_STATEINFO_H

#include "PlasMOUL/Key.h"
#include "PlasMOUL/creatable.h"
#include "Types/UnifiedTime.h"
#include "Types/Math.h"
#include "Types/Color.h"
#include "strings.h"

#define SDL_IOVERSION (6)

namespace SDL
{
    enum VarType
    {
        e_VarInt, e_VarFloat, e_VarBool, e_VarString, e_VarKey, e_VarCreatable,
        e_VarDouble, e_VarTime, e_VarByte, e_VarShort, e_VarAgeTimeOfDay,
        e_VarVector3, e_VarPoint3, e_VarQuaternion, e_VarRgb, e_VarRgba,
        e_VarRgb8, e_VarRgba8, e_VarStateDesc,
    };

    struct StateDescriptor;
    struct VarDescriptor;

    class Variable
    {
    public:
        enum Contents
        {
            e_HasUoid             = (1<<0),
            e_HasNotificationInfo = (1<<1),
            e_HasTimeStamp        = (1<<2),
            e_SameAsDefault       = (1<<3),
            e_HasDirtyFlag        = (1<<4),
            e_WantTimeStamp       = (1<<5),

            /* Extended (not saved) flags */
            e_XIsDirty            = (1<<8),
        };

        Variable(VarDescriptor* desc = 0) : m_data(0)
        {
            if (desc)
                m_data = new _ref(desc);
        }

        Variable(const Variable& copy)
            : m_data(copy.m_data)
        {
            if (m_data)
                m_data->ref();
        }

        ~Variable()
        {
            if (m_data)
                m_data->unref();
        }

        Variable& operator=(const Variable& copy)
        {
            if (copy.m_data)
                copy.m_data->ref();
            if (m_data)
                m_data->unref();
            m_data = copy.m_data;
            return *this;
        }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

#ifdef DEBUG
        void debug();
#endif

        void setDefault();
        bool isDefault() const;

    private:
        struct _ref
        {
            union
            {
                int32_t* m_int;
                int16_t* m_short;
                int8_t* m_byte;
                float* m_float;
                double* m_double;
                bool* m_bool;

                DS::String* m_string;
                DS::UnifiedTime* m_time;
                DS::Vector3* m_vector;
                DS::Quaternion* m_quat;
                DS::ColorRgba* m_color;
                DS::ColorRgba8* m_color8;

                MOUL::Uoid* m_key;
                MOUL::Creatable** m_creatable;
                class State* m_child;
            };
            size_t m_size;
            DS::UnifiedTime m_timestamp;
            DS::String m_notificationHint;
            uint16_t m_flags;

        private:
            int m_refs;
            VarDescriptor* m_desc;

            _ref(VarDescriptor* desc);
            ~_ref() { clear(); }

            void ref() { ++m_refs; }
            void unref()
            {
                if (--m_refs == 0)
                    delete this;
            }

            void resize(size_t size);
            void clear();

            void read(DS::Stream* stream);
            void write(DS::Stream* stream);

            friend class Variable;
        }* m_data;

    public:
        VarDescriptor* descriptor() const { return m_data ? m_data->m_desc : 0; }
        _ref* data() const { return m_data; }
    };

    class State
    {
    public:
        State(StateDescriptor* desc = 0);

        State(const State& copy) : m_data(copy.m_data)
        {
            if (m_data)
                m_data->ref();
        }

        ~State()
        {
            if (m_data)
                m_data->unref();
        }

        State& operator=(const State& copy)
        {
            if (copy.m_data)
                copy.m_data->ref();
            if (m_data)
                m_data->unref();
            m_data = copy.m_data;
            return *this;
        }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream) const;
        void add(const State& state);
        void merge(const State& state);

#ifdef DEBUG
        void debug();
#endif

        void setDefault();
        bool isDefault() const;
        bool isDirty() const;

        static State Create(DS::Stream* stream);

        DS::Blob toBlob() const;
        static State FromBlob(const DS::Blob& blob)
        {
            if (!blob.size())
                return State();

            DS::BlobStream bs(blob);
            State state = Create(&bs);
            state.read(&bs);
#ifdef DEBUG
            if (!bs.atEof())
                fprintf(stderr, "[SDL] Did not fully parse SDL blob! (@0x%x)\n", bs.tell());
#endif
            return state;
        }

    private:
        struct _ref
        {
            int m_refs;
            StateDescriptor* m_desc;
            std::vector<Variable> m_vars;
            std::vector<Variable*> m_simpleVars, m_sdVars;
            MOUL::Uoid m_object;
            uint16_t m_flags;

            _ref(StateDescriptor* desc)
                : m_refs(1), m_desc(desc), m_flags(0) { }

            void ref() { ++m_refs; }
            void unref()
            {
                if (--m_refs == 0)
                    delete this;
            }
        }* m_data;

    public:
        StateDescriptor* descriptor() const { return m_data ? m_data->m_desc : 0; }
        _ref* data() const { return m_data; }
    };
}

#endif
