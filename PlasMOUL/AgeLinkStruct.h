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

#ifndef _MOUL_AGELINKSTRUCT_H
#define _MOUL_AGELINKSTRUCT_H

#include "creatable.h"
#include "strings.h"
#include "Types/BitVector.h"
#include "Types/Uuid.h"

#define UPDATEFLAG(flag, on) \
    if (on) \
        m_flags |= flag; \
    else \
        m_flags &= ~flag;

namespace MOUL
{
    class SpawnPointInfo
    {
    public:
        enum
        {
            e_HasTitle, e_HasName, e_HasCameraStack
        };
        
        void read(DS::Stream* s);
        void write(DS::Stream* s);
        
        DS::String title() const { return m_title; }
        void title(DS::String title)
        {
            m_title = title;
            m_flags.set(e_HasTitle, !title.isEmpty());
        }
        
        DS::String name() const { return m_spawnPt; }
        void name(DS::String name)
        {
            m_spawnPt = name;
            m_flags.set(e_HasName, !name.isEmpty());
        }
        
        DS::String cameraStack() const { return m_cameraStack; }
        void cameraStack(DS::String stack)
        {
            m_cameraStack = stack;
            m_flags.set(e_HasCameraStack, !stack.isEmpty());
        }
        
    protected:
        DS::BitVector m_flags;
        DS::String m_title, m_spawnPt, m_cameraStack;
    };
    
    class AgeInfoStruct : public Creatable
    {
        FACTORY_CREATABLE(AgeInfoStruct)
        
        virtual void read(DS::Stream* s);
        virtual void write(DS::Stream* s);
        
    public:
        DS::String filename() const { return m_ageFilename; }
        void filename(DS::String filename)
        {
            m_ageFilename = filename;
            UPDATEFLAG(e_HasAgeFilename, !filename.isEmpty());
        }
        
        DS::String instanceName() const { return m_ageInstanceName; }
        void instanceName(DS::String name)
        {
            m_ageInstanceName = name;
            UPDATEFLAG(e_HasAgeInstanceName, !name.isEmpty());
        }
        
        DS::Uuid instanceUuid() const { return m_ageInstanceUuid; }
        void instanceUuid(DS::Uuid uuid)
        {
            m_ageInstanceUuid = uuid;
            UPDATEFLAG(e_HasAgeInstanceUuid, !uuid.isNull());
        }
        
        DS::String userDefinedName() const { return m_ageUserDefinedName; }
        void userDefinedName(DS::String name)
        {
            m_ageUserDefinedName = name;
            UPDATEFLAG(e_HasAgeUserDefinedName, !name.isEmpty());
        }
        
        int32_t sequenceNumber() const { return m_ageSequenceNumber; }
        void sequenceNumber(int32_t seq)
        {
            m_ageSequenceNumber = seq;
            UPDATEFLAG(e_HasAgeSequenceNumber, seq != 0);
        }
        
        DS::String description() const { return m_ageDescription; }
        void description(DS::String description)
        {
            m_ageDescription = description;
            UPDATEFLAG(e_HasAgeDescription, !description.isEmpty());
        }
        
        int32_t language() const { return m_ageLanguage; }
        void language(int32_t language)
        {
            m_ageLanguage = language;
            UPDATEFLAG(e_HasAgeLanguage, language >= 0);
        }
        
    protected:
        enum
        {
            e_HasAgeFilename = 1<<0,
            e_HasAgeInstanceName = 1<<1,
            e_HasAgeInstanceUuid = 1<<2,
            e_HasAgeUserDefinedName = 1<<3,
            e_HasAgeSequenceNumber = 1<<4,
            e_HasAgeDescription = 1<<5,
            e_HasAgeLanguage = 1<<6
        };
        
        uint8_t m_flags;
        DS::String m_ageFilename, m_ageInstanceName;
        DS::Uuid m_ageInstanceUuid;
        DS::String m_ageUserDefinedName;
        int32_t m_ageSequenceNumber;
        DS::String m_ageDescription;
        int32_t m_ageLanguage;
        
        AgeInfoStruct(uint16_t type) : Creatable(type), m_flags(0), m_ageSequenceNumber(0), m_ageLanguage(-1) { }
    };
    
    class AgeLinkStruct : public Creatable
    {
        FACTORY_CREATABLE(AgeLinkStruct)
        
        virtual void read(DS::Stream* s);
        virtual void write(DS::Stream* s);
        
    public:
        SpawnPointInfo* spawnPt()
        {
            if (!m_spawnPt) {
                m_spawnPt = new SpawnPointInfo;
                m_flags |= e_HasSpawnPt;
            }
            return m_spawnPt;
        }
        
        AgeInfoStruct* ageInfo()
        {
            if (!m_ageInfo) {
                m_ageInfo = AgeInfoStruct::Create();
                m_flags |= e_HasAgeInfo;
            }
            return m_ageInfo;
        }
        
        uint8_t linkingRules() const { return m_linkingRules; }
        void linkingRules(uint8_t rules)
        {
            m_linkingRules = rules;
            m_flags |= e_HasLinkingRules;
        }
        
        bool amCCR() const { return m_amCcr; }
        void amCCR(bool ccr)
        {
            m_amCcr = ccr;
            m_flags |= e_HasAmCCR;
        }
        
        DS::String parentAgeFilename() const { return m_parentAgeFilename; }
        void parentAgeFilename(DS::String filename)
        {
            m_parentAgeFilename = filename;
            UPDATEFLAG(e_HasParentAgeFilename, !filename.isEmpty());
        }
        
    protected:
        enum
        {
            e_HasAgeInfo = 1<<0,
            e_HasLinkingRules = 1<<1,
            e_HasSpawnPtInline = 1<<2, // DEAD
            e_HasSpawnPtLegacy = 1<<3, // DEAD
            e_HasAmCCR = 1<<4,
            e_HasSpawnPt = 1<<5,
            e_HasParentAgeFilename = 1<<6
        };
        
        uint16_t m_flags;
        AgeInfoStruct* m_ageInfo;
        uint8_t m_linkingRules;
        SpawnPointInfo* m_spawnPt;
        bool m_amCcr;
        DS::String m_parentAgeFilename;
        
        AgeLinkStruct(uint16_t type) : Creatable(type), m_flags(e_HasSpawnPt), m_ageInfo(0), m_linkingRules(0), m_amCcr(false) { }
        ~AgeLinkStruct();
    };
};

#undef UPDATEFLAG

#endif // _MOUL_AGELINKSTRUCT_H
