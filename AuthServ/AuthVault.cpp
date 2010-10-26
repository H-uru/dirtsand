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

#include "AuthServer_Private.h"
#include "VaultTypes.h"
#include "SDL/DescriptorDb.h"
#include "encodings.h"
#include "errors.h"
#include <ctime>

static uint32_t s_systemNode = 0;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

static inline void check_postgres()
{
    if (PQstatus(s_postgres) == CONNECTION_BAD)
        PQreset(s_postgres);
    DS_DASSERT(PQstatus(s_postgres) == CONNECTION_OK);
}

static DS::Blob gen_default_sdl(const DS::String& filename)
{
    SDL::StateDescriptor* desc = SDL::DescriptorDb::FindDescriptor(filename, -1);
    if (!desc) {
        fprintf(stderr, "[Vault] Warning: Could not find SDL descriptor for %s\n",
                filename.c_str());
        return DS::Blob();
    }

    SDL::State state(desc);
    DS::BufferStream stream;
    state.write(&stream);
    return DS::Blob(stream.buffer(), stream.size());
}

bool init_vault()
{
    PostgresStrings<1> sparm;
    sparm.set(0, DS::Vault::e_NodeSystem);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT \"idx\" FROM vault.\"Nodes\""
            "    WHERE \"NodeType\"=$1",
            1, 0, sparm.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return false;
    }

    int count = PQntuples(result);
    if (count == 0) {
        PQclear(result);

        fprintf(stderr, "[Vault] Initializing empty DirtSand vault\n");

        // Create system and global inbox nodes
        DS::Vault::Node node;
        node.set_NodeType(DS::Vault::e_NodeSystem);
        s_systemNode = v_create_node(node);
        if (s_systemNode == 0)
            return false;

        node.set_NodeType(DS::Vault::e_NodeFolder);
        node.set_Int32_1(DS::Vault::e_GlobalInboxFolder);
        uint32_t globalInbox = v_create_node(node);
        if (globalInbox == 0)
            return false;

        if (!v_ref_node(s_systemNode, globalInbox, 0))
            return false;

        if (v_create_age(gen_uuid(), "city", "Ae'gura", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "Neighborhood02", "Kirel", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "Kveer", "K'veer", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "GreatTreePub", "The Watcher's Pub", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "GuildPub-Cartographers", "The Cartographers' Guild Pub", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "GuildPub-Greeters", "The Greeters' Guild Pub", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "GuildPub-Maintainers", "The Maintainers' Guild Pub", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "GuildPub-Messengers", "The Messengers' Guild Pub", "", 0, true) == 0)
            return false;
        if (v_create_age(gen_uuid(), "GuildPub-Writers", "The Writers' Guild Pub", "", 0, true) == 0)
            return false;
    } else {
        DS_DASSERT(count == 1);
        s_systemNode = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);
    }

    return true;
}

DS::Uuid gen_uuid()
{
    check_postgres();
    PGresult* result = PQexec(s_postgres, "SELECT uuid_generate_v4()");
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::Uuid();
    }
    DS_DASSERT(PQntuples(result) == 1);
    DS::Uuid uuid(PQgetvalue(result, 0, 0));
    PQclear(result);
    return uuid;
}

uint32_t v_create_age(DS::Uuid ageId, DS::String filename, DS::String instName,
                      DS::String userName, int32_t seqNumber, bool publicAge)
{
    DS::Vault::Node node;
    node.set_NodeType(DS::Vault::e_NodeAge);
    node.set_CreatorUuid(ageId);
    node.set_Uuid_1(ageId);
    node.set_String64_1(filename);
    uint32_t ageNode = v_create_node(node);
    if (ageNode == 0)
        return 0;

    // TODO: Global SDL node

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_ChronicleFolder);
    uint32_t chronFolder = v_create_node(node);
    if (chronFolder == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_PeopleIKnowAboutFolder);
    uint32_t knownFolder = v_create_node(node);
    if (knownFolder == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_SubAgesFolder);
    uint32_t subAgesFolder = v_create_node(node);
    if (subAgesFolder == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfo);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(seqNumber);
    node.set_Int32_2(publicAge ? 1 : 0);
    node.set_Int32_3(-1);   // Language
    node.set_Uint32_1(ageNode);
    node.set_Uint32_2(0);   // Czar ID
    node.set_Uint32_3(0);   // Flags
    node.set_Uuid_1(ageId);
    node.set_String64_2(filename);
    node.set_String64_3(instName);
    if (!userName.isEmpty()) {
        node.set_String64_4(userName);
        node.set_Text_1(userName + " " + instName);
    }
    uint32_t ageInfoNode = v_create_node(node);
    if (ageInfoNode == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_AgeDevicesFolder);
    uint32_t devsFolder = v_create_node(node);
    if (devsFolder == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_CanVisitFolder);
    uint32_t canVisitList = v_create_node(node);
    if (canVisitList == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeSDL);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(0);
    node.set_String64_1(filename);
    node.set_Blob_1(gen_default_sdl(filename));
    uint32_t ageSdlNode = v_create_node(node);
    if (ageSdlNode == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_AgeOwnersFolder);
    uint32_t ageOwners = v_create_node(node);
    if (ageOwners == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_ChildAgesFolder);
    uint32_t childAges = v_create_node(node);
    if (childAges == 0)
        return 0;

    if (!v_ref_node(ageNode, s_systemNode, 0))
        return 0;
    if (!v_ref_node(ageNode, chronFolder, 0))
        return 0;
    if (!v_ref_node(ageNode, knownFolder, 0))
        return 0;
    if (!v_ref_node(ageNode, subAgesFolder, 0))
        return 0;
    if (!v_ref_node(ageNode, ageInfoNode, 0))
        return 0;
    if (!v_ref_node(ageNode, devsFolder, 0))
        return 0;
    if (!v_ref_node(ageInfoNode, canVisitList, 0))
        return 0;
    if (!v_ref_node(ageInfoNode, ageSdlNode, 0))
        return 0;
    if (!v_ref_node(ageInfoNode, ageOwners, 0))
        return 0;
    if (!v_ref_node(ageInfoNode, childAges, 0))
        return 0;

    // Register with the database if it's a public age
    if (publicAge) {
        PostgresStrings<8> parms;
        parms.set(0, ageId.toString());
        parms.set(1, filename);
        parms.set(2, instName);
        parms.set(3, userName);
        parms.set(4, userName.isEmpty() ? "" : userName + " " + instName);
        parms.set(5, seqNumber);
        parms.set(6, -1);   // Language
        parms.set(7, 0);    // Population
        PGresult* result = PQexecParams(s_postgres,
                "INSERT INTO game.\"PublicAges\""
                "    (\"AgeUuid\", \"AgeFilename\", \"AgeInstName\", \"AgeUserName\","
                "     \"AgeDesc\", \"SeqNumber\", \"Language\", \"Population\")"
                "    VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
                8, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return 0;
        }
        PQclear(result);
    }

    return ageNode;
}

uint32_t v_create_player(DS::Uuid playerId, DS::String playerName,
                         DS::String avatarShape, bool explorer)
{
    DS::Vault::Node node;
    node.set_NodeType(DS::Vault::e_NodePlayer);
    node.set_CreatorUuid(playerId);
    node.set_Int32_2(explorer ? 1 : 0);
    node.set_Uuid_1(playerId);
    node.set_String64_1(avatarShape);
    node.set_IString64_1(playerName);
    uint32_t playerNode = v_create_node(node);
    if (playerNode == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfo);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Uint32_1(playerNode);
    node.set_IString64_1(playerName);
    uint32_t playerInfoNode = v_create_node(node);
    if (playerInfoNode == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_BuddyListFolder);
    uint32_t buddyList = v_create_node(node);
    if (buddyList == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_IgnoreListFolder);
    uint32_t ignoreList = v_create_node(node);
    if (ignoreList == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_PlayerInviteFolder);
    uint32_t invites = v_create_node(node);
    if (invites == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_AgesIOwnFolder);
    uint32_t agesNode = v_create_node(node);
    if (agesNode == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_AgeJournalsFolder);
    uint32_t journals = v_create_node(node);
    if (journals == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_ChronicleFolder);
    uint32_t chronicles = v_create_node(node);
    if (chronicles == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_AgesICanVisitFolder);
    uint32_t visitFolder = v_create_node(node);
    if (visitFolder == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_AvatarOutfitFolder);
    uint32_t outfit = v_create_node(node);
    if (outfit == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_AvatarClosetFolder);
    uint32_t closet = v_create_node(node);
    if (closet == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_InboxFolder);
    uint32_t inbox = v_create_node(node);
    if (inbox == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Int32_1(DS::Vault::e_PeopleIKnowAboutFolder);
    uint32_t peopleNode = v_create_node(node);
    if (peopleNode == 0)
        return 0;

    DS::Blob link(reinterpret_cast<const uint8_t*>("Default:LinkInPointDefault:;"),
                  strlen("Default:LinkInPointDefault:;"));
    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeLink);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Blob_1(link);
    uint32_t reltoLink = v_create_node(node);
    if (reltoLink == 0)
        return 0;

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeLink);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Blob_1(link);
    uint32_t hoodLink = v_create_node(node);
    if (hoodLink == 0)
        return 0;

    link = DS::Blob(reinterpret_cast<const uint8_t*>("Ferry Terminal:LinkInPointFerry:;"),
                    strlen("Ferry Terminal:LinkInPointFerry:;"));
    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeLink);
    node.set_CreatorUuid(playerId);
    node.set_CreatorIdx(playerNode);
    node.set_Blob_1(link);
    uint32_t cityLink = v_create_node(node);
    if (hoodLink == 0)
        return 0;

    uint32_t reltoAge = v_create_age(gen_uuid(), "Personal", "Relto",
                                     playerName + "'s", 0, false);
    if (reltoAge == 0)
        return 0;

    if (!v_ref_node(playerNode, s_systemNode, 0))
        return 0;
    if (!v_ref_node(playerNode, playerInfoNode, 0))
        return 0;
    if (!v_ref_node(playerNode, buddyList, 0))
        return 0;
    if (!v_ref_node(playerNode, ignoreList, 0))
        return 0;
    if (!v_ref_node(playerNode, invites, 0))
        return 0;
    if (!v_ref_node(playerNode, agesNode, 0))
        return 0;
    if (!v_ref_node(playerNode, journals, 0))
        return 0;
    if (!v_ref_node(playerNode, chronicles, 0))
        return 0;
    if (!v_ref_node(playerNode, visitFolder, 0))
        return 0;
    if (!v_ref_node(playerNode, outfit, 0))
        return 0;
    if (!v_ref_node(playerNode, closet, 0))
        return 0;
    if (!v_ref_node(playerNode, inbox, 0))
        return 0;
    if (!v_ref_node(playerNode, peopleNode, 0))
        return 0;
    if (!v_ref_node(agesNode, reltoLink, 0))
        return 0;
    if (!v_ref_node(agesNode, hoodLink, 0))
        return 0;
    if (!v_ref_node(agesNode, cityLink, 0))
        return 0;
    if (!v_ref_node(reltoLink, reltoAge, 0))
        return 0;
    //if (!v_ref_node(hoodLink, ..., 0))
    //    return 0;
    //if (!v_ref_node(cityLink, ..., 0))
    //    return 0;

    return playerNode;
}

uint32_t v_create_node(const DS::Vault::Node& node)
{
    /* This should be plenty to store everything we need without a bunch
     * of dynamic reallocations
     */
    PostgresStrings<32> parms;
    char fieldbuf[1024];

    size_t parmcount = 0;
    char* fieldp = fieldbuf;

    #define SET_FIELD(name, value) \
        { \
            parms.set(parmcount++, value); \
            strcpy(fieldp, "\"" #name "\","); \
            fieldp += strlen("\"" #name "\","); \
        }
    int now = time(0);
    SET_FIELD(CreateTime, now);
    SET_FIELD(ModifyTime, now);
    if (node.has_CreateAgeName())
        SET_FIELD(CreateAgeName, node.m_CreateAgeName);
    if (node.has_CreateAgeUuid())
        SET_FIELD(CreateAgeUuid, node.m_CreateAgeUuid.toString());
    if (node.has_CreatorUuid())
        SET_FIELD(CreatorUuid, node.m_CreatorUuid.toString());
    if (node.has_CreatorIdx())
        SET_FIELD(CreatorIdx, node.m_CreatorIdx);
    if (node.has_NodeType())
        SET_FIELD(NodeType, node.m_NodeType);
    if (node.has_Int32_1())
        SET_FIELD(Int32_1, node.m_Int32_1);
    if (node.has_Int32_2())
        SET_FIELD(Int32_2, node.m_Int32_2);
    if (node.has_Int32_3())
        SET_FIELD(Int32_3, node.m_Int32_3);
    if (node.has_Int32_4())
        SET_FIELD(Int32_4, node.m_Int32_4);
    if (node.has_Uint32_1())
        SET_FIELD(Uint32_1, node.m_Uint32_1);
    if (node.has_Uint32_2())
        SET_FIELD(Uint32_2, node.m_Uint32_2);
    if (node.has_Uint32_3())
        SET_FIELD(Uint32_3, node.m_Uint32_3);
    if (node.has_Uint32_4())
        SET_FIELD(Uint32_4, node.m_Uint32_4);
    if (node.has_Uuid_1())
        SET_FIELD(Uuid_1, node.m_Uuid_1.toString());
    if (node.has_Uuid_2())
        SET_FIELD(Uuid_2, node.m_Uuid_2.toString());
    if (node.has_Uuid_3())
        SET_FIELD(Uuid_3, node.m_Uuid_3.toString());
    if (node.has_Uuid_4())
        SET_FIELD(Uuid_4, node.m_Uuid_4.toString());
    if (node.has_String64_1())
        SET_FIELD(String64_1, node.m_String64_1);
    if (node.has_String64_2())
        SET_FIELD(String64_2, node.m_String64_2);
    if (node.has_String64_3())
        SET_FIELD(String64_3, node.m_String64_3);
    if (node.has_String64_4())
        SET_FIELD(String64_4, node.m_String64_4);
    if (node.has_String64_5())
        SET_FIELD(String64_5, node.m_String64_5);
    if (node.has_String64_6())
        SET_FIELD(String64_6, node.m_String64_6);
    if (node.has_IString64_1())
        SET_FIELD(IString64_1, node.m_IString64_1);
    if (node.has_IString64_2())
        SET_FIELD(IString64_2, node.m_IString64_2);
    if (node.has_Text_1())
        SET_FIELD(Text_1, node.m_Text_1);
    if (node.has_Text_2())
        SET_FIELD(Text_2, node.m_Text_2);
    if (node.has_Blob_1())
        SET_FIELD(Blob_1, DS::Base64Encode(node.m_Blob_1.buffer(), node.m_Blob_1.size()));
    if (node.has_Blob_2())
        SET_FIELD(Blob_2, DS::Base64Encode(node.m_Blob_2.buffer(), node.m_Blob_2.size()));
    #undef SET_FIELD

    DS_DASSERT(fieldp - fieldbuf < 1024);
    *(fieldp - 1) = ')';    // Get rid of the last comma
    DS::String queryStr = "INSERT INTO vault.\"Nodes\" (";
    queryStr += fieldbuf;

    fieldp = fieldbuf;
    for (size_t i=0; i<parmcount; ++i) {
        sprintf(fieldp, "$%u,", i+1);
        fieldp += strlen(fieldp);
    }
    DS_DASSERT(fieldp - fieldbuf < 1024);
    *(fieldp - 1) = ')';    // Get rid of the last comma
    queryStr += "\n    VALUES (";
    queryStr += fieldbuf;
    queryStr += "\n    RETURNING idx";

    check_postgres();
    PGresult* result = PQexecParams(s_postgres, queryStr.c_str(),
                                    parmcount, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return 0;
    }
    DS_DASSERT(PQntuples(result) == 1);
    uint32_t idx = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);
    return idx;
}

bool v_ref_node(uint32_t parentIdx, uint32_t childIdx, uint32_t ownerIdx)
{
    PostgresStrings<3> parms;
    parms.set(0, parentIdx);
    parms.set(1, childIdx);
    parms.set(2, ownerIdx);
    PGresult* result = PQexecParams(s_postgres,
        "INSERT INTO vault.\"NodeRefs\""
        "    (\"ParentIdx\", \"ChildIdx\", \"OwnerIdx\")"
        "    VALUES ($1, $2, $3)",
        3, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return false;
    }
    PQclear(result);
    return true;
}
