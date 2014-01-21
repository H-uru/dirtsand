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

#include "AgeLinkStruct.h"
#include "errors.h"

void MOUL::SpawnPointInfo::read(DS::Stream* s)
{
    m_flags.read(s);
    if (m_flags.get(e_HasTitle))
        m_title = s->readPString<uint16_t>();
    if (m_flags.get(e_HasName))
        m_spawnPt = s->readPString<uint16_t>();
    if (m_flags.get(e_HasCameraStack))
        m_cameraStack = s->readPString<uint16_t>();
}

void MOUL::SpawnPointInfo::write(DS::Stream* s) const
{
    m_flags.write(s);
    if (m_flags.get(e_HasTitle))
        s->writePString<uint16_t>(m_title);
    if (m_flags.get(e_HasName))
        s->writePString<uint16_t>(m_spawnPt);
    if (m_flags.get(e_HasCameraStack))
        s->writePString<uint16_t>(m_cameraStack);
}

void MOUL::AgeInfoStruct::read(DS::Stream* s)
{
    m_flags = s->read<uint8_t>();
    if (m_flags & e_HasAgeFilename)
        m_ageFilename = s->readPString<uint16_t>();
    if (m_flags & e_HasAgeInstanceName)
        m_ageInstanceName = s->readPString<uint16_t>();
    if (m_flags & e_HasAgeInstanceUuid)
        m_ageInstanceUuid.read(s);
    if (m_flags & e_HasAgeUserDefinedName)
        m_ageUserDefinedName = s->readPString<uint16_t>();
    if (m_flags & e_HasAgeSequenceNumber)
        m_ageSequenceNumber = s->read<int32_t>();
    if (m_flags & e_HasAgeDescription)
        m_ageDescription = s->readPString<uint16_t>();
    if (m_flags & e_HasAgeLanguage)
        m_ageLanguage = s->read<int32_t>();
}

void MOUL::AgeInfoStruct::write(DS::Stream* s) const
{
    s->write<uint8_t>(m_flags);
    if (m_flags & e_HasAgeFilename)
        s->writePString<uint16_t>(m_ageFilename);
    if (m_flags & e_HasAgeInstanceName)
        s->writePString<uint16_t>(m_ageInstanceName);
    if (m_flags & e_HasAgeInstanceUuid)
        m_ageInstanceUuid.write(s);
    if (m_flags & e_HasAgeUserDefinedName)
        s->writePString<uint16_t>(m_ageUserDefinedName);
    if (m_flags & e_HasAgeSequenceNumber)
        s->write<int32_t>(m_ageSequenceNumber);
    if (m_flags & e_HasAgeDescription)
        s->writePString<uint16_t>(m_ageDescription);
    if (m_flags & e_HasAgeLanguage)
        s->write<int32_t>(m_ageLanguage);
}

void MOUL::AgeLinkStruct::read(DS::Stream* s)
{
    m_flags = s->read<uint16_t>();
    DS_PASSERT(!(m_flags & e_HasSpawnPtInline)); // Trolololololololo
    DS_PASSERT(!(m_flags & e_HasSpawnPtLegacy)); // Ahahahahahahahaha

    if (m_flags & e_HasAgeInfo)
        m_ageInfo->read(s);
    if (m_flags & e_HasLinkingRules)
        m_linkingRules = s->read<uint8_t>();
    if (m_flags & e_HasSpawnPt)
        m_spawnPt.read(s);
    if (m_flags & e_HasAmCCR)
        m_amCcr = s->read<bool>();
    if (m_flags & e_HasParentAgeFilename)
        m_parentAgeFilename = s->readPString<uint16_t>();
}

void MOUL::AgeLinkStruct::write(DS::Stream* s) const
{
    DS_PASSERT(!(m_flags & e_HasSpawnPtInline)); // Trolololololololo
    DS_PASSERT(!(m_flags & e_HasSpawnPtLegacy)); // Ahahahahahahahaha
    s->write<uint16_t>(m_flags);

    if (m_flags & e_HasAgeInfo)
        m_ageInfo->write(s);
    if (m_flags & e_HasLinkingRules)
        s->write<uint8_t>(m_linkingRules);
    if (m_flags & e_HasSpawnPt)
        m_spawnPt.write(s);
    if (m_flags & e_HasAmCCR)
        s->write<bool>(m_amCcr);
    if (m_flags & e_HasParentAgeFilename)
        s->writePString<uint16_t>(m_parentAgeFilename);
}
