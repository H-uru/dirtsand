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

#ifndef _DS_VAULTTYPES_H
#define _DS_VAULTTYPES_H

#include "Types/Uuid.h"

namespace DS
{
namespace Vault
{
    enum NodeType
    {
        e_NodeInvalid, e_NodeVNodeMgrLow, e_NodePlayer, e_NodeAge,
        e_NodeGameServer, e_NodeAdmin, e_NodeVaultServer, e_NodeCCR,
        e_NodeVNodeMgrHigh = 21, e_NodeFolder, e_NodePlayerInfo, e_NodeSystem,
        e_NodeImage, e_NodeTextNote, e_NodeSDL, e_NodeAgeLink, e_NodeChronicle,
        e_NodePlayerInfoList, e_NodeUNUSED, e_NodeMarker, e_NodeAgeInfo,
        e_NodeAgeInfoList, e_NodeMarkerList
    };

    enum StandardNodes
    {
        e_UserDefinedNode, e_InboxFolder, e_BuddyListFolder, e_IgnoreListFolder,
        e_PeopleIKnowAboutFolder, e_VaultMgrGlobalDataFolder, e_ChronicleFolder,
        e_AvatarOutfitFolder, e_AgeTypeJournalFolder, e_SubAgesFolder,
        e_DeviceInboxFolder, e_HoodMembersFolder, e_AllPlayersFolder,
        e_AgeMembersFolder, e_AgeJournalsFolder, e_AgeDevicesFolder,
        e_AgeInstanceSDLNode, e_AgeGlobalSDLNode, e_CanVisitFolder,
        e_AgeOwnersFolder, e_AllAgeGlobalSDLNodesFolder, e_PlayerInfoNode,
        e_PublicAgesFolder, e_AgesIOwnFolder, e_AgesICanVisitFolder,
        e_AvatarClosetFolder, e_AgeInfoNode, e_SystemNode, e_PlayerInviteFolder,
        e_CCRPlayersFolder, e_GlobalInboxFolder, e_ChildAgesFolder,
        e_GameScoresFolder
    };

    enum NoteTypes
    {
        e_NoteGeneric, e_NoteCCRPetition, e_NoteDevice, e_NoteInvite,
        e_NoteVisit, e_NoteUnVisit
    };

    enum ImgTypes
    {
        e_ImgNone, e_ImgJpeg
    };

    enum NodeFields
    {
        e_FieldNodeIdx          = (1ULL<<0),
        e_FieldCreateTime       = (1ULL<<1),
        e_FieldModifyTime       = (1ULL<<2),
        e_FieldCreateAgeName    = (1ULL<<3),
        e_FieldCreateAgeUuid    = (1ULL<<4),
        e_FieldCreatorUuid      = (1ULL<<5),
        e_FieldCreatorIdx       = (1ULL<<6),
        e_FieldNodeType         = (1ULL<<7),
        e_FieldInt32_1          = (1ULL<<8),
        e_FieldInt32_2          = (1ULL<<9),
        e_FieldInt32_3          = (1ULL<<10),
        e_FieldInt32_4          = (1ULL<<11),
        e_FieldUint32_1         = (1ULL<<12),
        e_FieldUint32_2         = (1ULL<<13),
        e_FieldUint32_3         = (1ULL<<14),
        e_FieldUint32_4         = (1ULL<<15),
        e_FieldUuid_1           = (1ULL<<16),
        e_FieldUuid_2           = (1ULL<<17),
        e_FieldUuid_3           = (1ULL<<18),
        e_FieldUuid_4           = (1ULL<<19),
        e_FieldString64_1       = (1ULL<<20),
        e_FieldString64_2       = (1ULL<<21),
        e_FieldString64_3       = (1ULL<<22),
        e_FieldString64_4       = (1ULL<<23),
        e_FieldString64_5       = (1ULL<<24),
        e_FieldString64_6       = (1ULL<<25),
        e_FieldIString64_1      = (1ULL<<26),
        e_FieldIString64_2      = (1ULL<<27),
        e_FieldText_1           = (1ULL<<28),
        e_FieldText_2           = (1ULL<<29),
        e_FieldBlob_1           = (1ULL<<30),
        e_FieldBlob_2           = (1ULL<<31),
    };

#define NODE_FIELD(type, name) \
    type m_##name; \
    void set_##name(type value) \
    { \
        m_##name = value; \
        m_fields |= e_Field##name; \
    } \
    bool has_##name() const { return (m_fields & e_Field##name) != 0; } \
    void clear_##name() { m_fields &= ~e_Field##name; }

    class Node
    {
    public:
        NODE_FIELD(uint32_t,    NodeIdx         )
        NODE_FIELD(uint32_t,    CreateTime      )
        NODE_FIELD(uint32_t,    ModifyTime      )
        NODE_FIELD(DS::String,  CreateAgeName   )
        NODE_FIELD(DS::Uuid,    CreateAgeUuid   )
        NODE_FIELD(DS::Uuid,    CreatorUuid     )
        NODE_FIELD(uint32_t,    CreatorIdx      )
        NODE_FIELD(int32_t,     NodeType        )
        NODE_FIELD(int32_t,     Int32_1         )
        NODE_FIELD(int32_t,     Int32_2         )
        NODE_FIELD(int32_t,     Int32_3         )
        NODE_FIELD(int32_t,     Int32_4         )
        NODE_FIELD(uint32_t,    Uint32_1        )
        NODE_FIELD(uint32_t,    Uint32_2        )
        NODE_FIELD(uint32_t,    Uint32_3        )
        NODE_FIELD(uint32_t,    Uint32_4        )
        NODE_FIELD(DS::Uuid,    Uuid_1          )
        NODE_FIELD(DS::Uuid,    Uuid_2          )
        NODE_FIELD(DS::Uuid,    Uuid_3          )
        NODE_FIELD(DS::Uuid,    Uuid_4          )
        NODE_FIELD(DS::String,  String64_1      )
        NODE_FIELD(DS::String,  String64_2      )
        NODE_FIELD(DS::String,  String64_3      )
        NODE_FIELD(DS::String,  String64_4      )
        NODE_FIELD(DS::String,  String64_5      )
        NODE_FIELD(DS::String,  String64_6      )
        NODE_FIELD(DS::String,  IString64_1     )
        NODE_FIELD(DS::String,  IString64_2     )
        NODE_FIELD(DS::String,  Text_1          )
        NODE_FIELD(DS::String,  Text_2          )
        NODE_FIELD(DS::Blob,    Blob_1          )
        NODE_FIELD(DS::Blob,    Blob_2          )

        Node() : m_fields(0) { }
        void clear() { m_fields = 0; }
        bool isNull() const { return m_fields == 0; }

        void read(DS::Stream* stream);
        void write(DS::Stream* stream);

    private:
        uint64_t m_fields;
    };

    struct NodeRef
    {
        uint32_t m_parent, m_child, m_owner;
        uint8_t m_seen;
    };
}
}

#endif
