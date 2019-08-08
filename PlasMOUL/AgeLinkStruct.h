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
        void write(DS::Stream* s) const;

        ST::string title() const { return m_title; }
        void setTitle(const ST::string& title)
        {
            m_title = title;
            m_flags.set(e_HasTitle, !title.empty());
        }

        ST::string name() const { return m_spawnPt; }
        void setName(const ST::string& name)
        {
            m_spawnPt = name;
            m_flags.set(e_HasName, !name.empty());
        }

        ST::string cameraStack() const { return m_cameraStack; }
        void setCameraStack(const ST::string& stack)
        {
            m_cameraStack = stack;
            m_flags.set(e_HasCameraStack, !stack.empty());
        }

    protected:
        DS::BitVector m_flags;
        ST::string m_title, m_spawnPt, m_cameraStack;
    };

    class AgeInfoStruct : public Creatable
    {
        FACTORY_CREATABLE(AgeInfoStruct)

        void read(DS::Stream* s) override;
        void write(DS::Stream* s) const override;

    public:
        ST::string filename() const { return m_ageFilename; }
        void setFilename(const ST::string& filename)
        {
            m_ageFilename = filename;
            UPDATEFLAG(e_HasAgeFilename, !filename.empty());
        }

        ST::string instanceName() const { return m_ageInstanceName; }
        void setInstanceName(const ST::string& name)
        {
            m_ageInstanceName = name;
            UPDATEFLAG(e_HasAgeInstanceName, !name.empty());
        }

        DS::Uuid instanceUuid() const { return m_ageInstanceUuid; }
        void setInstanceUuid(const DS::Uuid& uuid)
        {
            m_ageInstanceUuid = uuid;
            UPDATEFLAG(e_HasAgeInstanceUuid, !uuid.isNull());
        }

        ST::string userDefinedName() const { return m_ageUserDefinedName; }
        void setUserDefinedName(const ST::string& name)
        {
            m_ageUserDefinedName = name;
            UPDATEFLAG(e_HasAgeUserDefinedName, !name.empty());
        }

        int32_t sequenceNumber() const { return m_ageSequenceNumber; }
        void setSequenceNumber(int32_t seq)
        {
            m_ageSequenceNumber = seq;
            UPDATEFLAG(e_HasAgeSequenceNumber, seq != 0);
        }

        ST::string description() const { return m_ageDescription; }
        void setDescription(const ST::string& description)
        {
            m_ageDescription = description;
            UPDATEFLAG(e_HasAgeDescription, !description.empty());
        }

        int32_t language() const { return m_ageLanguage; }
        void setLanguage(int32_t language)
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
        ST::string m_ageFilename, m_ageInstanceName;
        DS::Uuid m_ageInstanceUuid;
        ST::string m_ageUserDefinedName;
        int32_t m_ageSequenceNumber;
        ST::string m_ageDescription;
        int32_t m_ageLanguage;

        AgeInfoStruct(uint16_t type)
            : Creatable(type), m_flags(), m_ageSequenceNumber(),
              m_ageLanguage(-1) { }
    };

    class AgeLinkStruct : public Creatable
    {
        FACTORY_CREATABLE(AgeLinkStruct)

        void read(DS::Stream* s) override;
        void write(DS::Stream* s) const override;

    public:
        SpawnPointInfo& spawnPt() { return m_spawnPt; }
        const SpawnPointInfo& spawnPt() const { return m_spawnPt; }

        uint8_t linkingRules() const { return m_linkingRules; }
        void setLinkingRules(uint8_t rules)
        {
            m_linkingRules = rules;
            m_flags |= e_HasLinkingRules;
        }

        bool amCCR() const { return m_amCcr; }
        void setAmCCR(bool ccr)
        {
            m_amCcr = ccr;
            m_flags |= e_HasAmCCR;
        }

        ST::string parentAgeFilename() const { return m_parentAgeFilename; }
        void setParentAgeFilename(const ST::string& filename)
        {
            m_parentAgeFilename = filename;
            UPDATEFLAG(e_HasParentAgeFilename, !filename.empty());
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
        SpawnPointInfo m_spawnPt;
        bool m_amCcr;
        ST::string m_parentAgeFilename;

        AgeLinkStruct(uint16_t type)
            : Creatable(type), m_flags(e_HasSpawnPt | e_HasAgeInfo),
              m_ageInfo(AgeInfoStruct::Create()), m_linkingRules(), m_amCcr() { }

        ~AgeLinkStruct() override { m_ageInfo->unref(); }
    };
};

#undef UPDATEFLAG

#endif // _MOUL_AGELINKSTRUCT_H
