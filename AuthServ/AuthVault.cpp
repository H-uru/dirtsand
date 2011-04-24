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
extern PGconn* s_postgres;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

static inline void check_postgres()
{
    if (PQstatus(s_postgres) == CONNECTION_BAD)
        PQreset(s_postgres);
    DS_DASSERT(PQstatus(s_postgres) == CONNECTION_OK);
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

DS::Blob gen_default_sdl(const DS::String& filename)
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

static uint32_t find_a_friendly_neighborhood_for_our_new_visitor(uint32_t playerInfoId)
{
    PGresult* result = PQexec(s_postgres,
            "SELECT \"AgeUuid\" FROM game.\"PublicAges\""
            "    WHERE \"AgeFilename\" = 'Neighborhood'"
            "    AND \"Population\" < 20 AND \"SeqNumber\" <> 0");
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return 0;
    }

    std::pair<uint32_t, uint32_t> ageNode;
    DS::Uuid ageId;
    if (PQntuples(result) != 0) {
        ageId = DS::Uuid(PQgetvalue(result, 0, 0));
        PQclear(result);

        PostgresStrings<2> parms;
        parms.set(0, DS::Vault::e_NodeAgeInfo);
        parms.set(1, ageId.toString());
        result = PQexecParams(s_postgres,
                "SELECT idx FROM vault.\"Nodes\""
                "    WHERE \"NodeType\"=$1 AND \"Uuid_1\"=$2",
                2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return 0;
        }
        DS_DASSERT(PQntuples(result) == 1);
        ageNode.second = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);
    } else {
        PQclear(result);
        AuthServer_AgeInfo age;
        age.m_ageId = gen_uuid();
        age.m_filename = "Neighborhood";
        age.m_instName = "Neighborhood";
        age.m_userName = "DS";
        age.m_description = "DS Neighborhood";
        age.m_seqNumber = -1;   // Auto-generate
        ageNode = v_create_age(age, e_AgePublic);
        if (ageNode.second == 0)
            return 0;
    }

    {
        PostgresStrings<2> parms;
        parms.set(0, ageNode.second);
        parms.set(1, DS::Vault::e_AgeOwnersFolder);
        result = PQexecParams(s_postgres,
                "SELECT idx FROM vault.find_folder($1, $2);",
                2, 0, parms.m_values, 0, 0, 0);
    }
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return 0;
    }
    DS_DASSERT(PQntuples(result) == 1);
    uint32_t ownerFolder = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);

    if (!v_ref_node(ownerFolder, playerInfoId, 0))
        return 0;

    {
        PostgresStrings<1> parms;
        parms.set(0, ageId.toString());
        result = PQexecParams(s_postgres,
                "UPDATE game.\"PublicAges\""
                "    SET \"Population\" = \"Population\"+1"
                "    WHERE \"AgeUuid\" = $1",
                1, 0, parms.m_values, 0, 0, 0);
    }
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return 0;
    }
    PQclear(result);

    return ageNode.second;
}

static uint32_t find_public_age_1(const DS::String& filename)
{
    PGresult* result;
    {
        PostgresStrings<1> parm;
        parm.set(0, filename);
        result = PQexecParams(s_postgres,
                "SELECT \"AgeUuid\" FROM game.\"PublicAges\""
                "    WHERE \"AgeFilename\"=$1 AND \"SeqNumber\"=0",
                1, 0, parm.m_values, 0, 0, 0);
    }
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return 0;
    }
    DS_DASSERT(PQntuples(result) == 1);

    DS::Uuid ageId(PQgetvalue(result, 0, 0));
    PQclear(result);

    {
        PostgresStrings<2> parms;
        parms.set(0, DS::Vault::e_NodeAgeInfo);
        parms.set(1, ageId.toString());
        result = PQexecParams(s_postgres,
                "SELECT idx FROM vault.\"Nodes\""
                "    WHERE \"NodeType\"=$1 AND \"Uuid_1\"=$2",
                2, 0, parms.m_values, 0, 0, 0);
    }
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return 0;
    }
    if (PQntuples(result) == 0) {
        // Public age not found
        PQclear(result);
        return 0;
    }
    DS_DASSERT(PQntuples(result) == 1);

    uint32_t ageInfoNode = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);
    return ageInfoNode;
}

bool dm_vault_init()
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

        AuthServer_AgeInfo age;
        age.m_ageId = gen_uuid();
        age.m_filename = "city";
        age.m_instName = "Ae'gura";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "Neighborhood02";
        age.m_instName = "Kirel";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "Kveer";
        age.m_instName = "K'veer";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "GreatTreePub";
        age.m_instName = "The Watcher's Pub";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "GuildPub-Cartographers";
        age.m_instName = "The Cartographers' Guild Pub";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "GuildPub-Greeters";
        age.m_instName = "The Greeters' Guild Pub";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "GuildPub-Maintainers";
        age.m_instName = "The Maintainers' Guild Pub";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "GuildPub-Messengers";
        age.m_instName = "The Messengers' Guild Pub";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;

        age.m_ageId = gen_uuid();
        age.m_filename = "GuildPub-Writers";
        age.m_instName = "The Writers' Guild Pub";
        if (v_create_age(age, e_AgePublic).first == 0)
            return false;
    } else {
        DS_DASSERT(count == 1);
        s_systemNode = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);
    }

    return true;
}

std::pair<uint32_t, uint32_t>
v_create_age(const AuthServer_AgeInfo& age, uint32_t flags)
{
    int seqNumber = age.m_seqNumber;
    if (seqNumber < 0) {
        check_postgres();

        PGresult* result = PQexec(s_postgres, "SELECT nextval('game.\"AgeSeqNumber\"'::regclass)");
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return std::make_pair(0, 0);
        }
        DS_DASSERT(PQntuples(result) == 1);
        seqNumber = strtol(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);
    }

    DS::Vault::Node node;
    node.set_NodeType(DS::Vault::e_NodeAge);
    node.set_CreatorUuid(age.m_ageId);
    node.set_Uuid_1(age.m_ageId);
    if (!age.m_parentId.isNull())
        node.set_Uuid_2(age.m_parentId);
    node.set_String64_1(age.m_filename);
    uint32_t ageNode = v_create_node(node);
    if (ageNode == 0)
        return std::make_pair(0, 0);

    // TODO: Global SDL node

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_ChronicleFolder);
    uint32_t chronFolder = v_create_node(node);
    if (chronFolder == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_PeopleIKnowAboutFolder);
    uint32_t knownFolder = v_create_node(node);
    if (knownFolder == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_SubAgesFolder);
    uint32_t subAgesFolder = v_create_node(node);
    if (subAgesFolder == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfo);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(seqNumber);
    node.set_Int32_2((flags & e_AgePublic) != 0 ? 1 : 0);
    node.set_Int32_3(age.m_language);
    node.set_Uint32_1(ageNode);
    node.set_Uint32_2(0);   // Czar ID
    node.set_Uint32_3(0);   // Flags
    node.set_Uuid_1(age.m_ageId);
    if (!age.m_parentId.isNull())
        node.set_Uuid_2(age.m_parentId);
    node.set_String64_2(age.m_filename);
    if (!age.m_instName.isNull())
        node.set_String64_3(age.m_instName);
    if (!age.m_userName.isEmpty())
        node.set_String64_4(age.m_userName);
    if (!age.m_description.isEmpty())
        node.set_Text_1(age.m_description);
    uint32_t ageInfoNode = v_create_node(node);
    if (ageInfoNode == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_AgeDevicesFolder);
    uint32_t devsFolder = v_create_node(node);
    if (devsFolder == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_CanVisitFolder);
    uint32_t canVisitList = v_create_node(node);
    if (canVisitList == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeSDL);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(0);
    node.set_String64_1(age.m_filename);
    node.set_Blob_1(gen_default_sdl(age.m_filename));
    uint32_t ageSdlNode = v_create_node(node);
    if (ageSdlNode == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_AgeOwnersFolder);
    uint32_t ageOwners = v_create_node(node);
    if (ageOwners == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(age.m_ageId);
    node.set_CreatorIdx(ageNode);
    node.set_Int32_1(DS::Vault::e_ChildAgesFolder);
    uint32_t childAges = v_create_node(node);
    if (childAges == 0)
        return std::make_pair(0, 0);

    if (!v_ref_node(ageNode, s_systemNode, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageNode, chronFolder, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageNode, knownFolder, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageNode, subAgesFolder, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageNode, ageInfoNode, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageNode, devsFolder, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageInfoNode, canVisitList, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageInfoNode, ageSdlNode, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageInfoNode, ageOwners, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(ageInfoNode, childAges, 0))
        return std::make_pair(0, 0);

    if (flags & e_AgeIsRelto) {
        node.clear();
        node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
        node.set_CreatorUuid(age.m_ageId);
        node.set_CreatorIdx(ageNode);
        node.set_Int32_1(DS::Vault::e_AgesIOwnFolder);
        uint32_t ownedAges = v_create_node(node);
        if (ownedAges == 0)
            return std::make_pair(0, 0);

        if (!v_ref_node(ageNode, ownedAges, 0))
            return std::make_pair(0, 0);
    }

    // Register with the server database
    {
        DS::String agedesc = !age.m_description.isEmpty() ? age.m_description
                           : !age.m_instName.isEmpty() ? age.m_instName
                           : age.m_filename;

        PostgresStrings<5> parms;
        parms.set(0, age.m_ageId.toString());
        parms.set(1, age.m_filename);
        parms.set(2, agedesc);
        parms.set(3, ageNode);
        parms.set(4, ageSdlNode);
        PGresult* result = PQexecParams(s_postgres,
                "INSERT INTO game.\"Servers\""
                "    (\"AgeUuid\", \"AgeFilename\", \"DisplayName\", \"AgeIdx\", \"SdlIdx\")"
                "    VALUES ($1, $2, $3, $4, $5)",
                5, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return std::make_pair(0, 0);
        }
        PQclear(result);
    }

    // Register with the database if it's a public age
    if (flags & e_AgePublic) {
        PostgresStrings<8> parms;
        parms.set(0, age.m_ageId.toString());
        parms.set(1, age.m_filename);
        parms.set(2, age.m_instName.isEmpty() ? "" : age.m_instName);
        parms.set(3, age.m_userName.isEmpty() ? "" : age.m_userName);
        parms.set(4, age.m_description.isEmpty() ? "" : age.m_description);
        parms.set(5, seqNumber);
        parms.set(6, age.m_language);
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
            return std::make_pair(0, 0);
        }
        PQclear(result);
    }

    return std::make_pair(ageNode, ageInfoNode);
}

std::pair<uint32_t, uint32_t>
v_create_player(DS::Uuid acctId, const AuthServer_PlayerInfo& player)
{
    DS::Vault::Node node;
    node.set_NodeType(DS::Vault::e_NodePlayer);
    node.set_CreatorUuid(acctId);
    node.set_Int32_2(player.m_explorer);
    node.set_Uuid_1(acctId);
    node.set_String64_1(player.m_avatarModel);
    node.set_IString64_1(player.m_playerName);
    uint32_t playerIdx = v_create_node(node);
    if (playerIdx == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfo);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Uint32_1(playerIdx);
    node.set_IString64_1(player.m_playerName);
    uint32_t playerInfoNode = v_create_node(node);
    if (playerInfoNode == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_BuddyListFolder);
    uint32_t buddyList = v_create_node(node);
    if (buddyList == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_IgnoreListFolder);
    uint32_t ignoreList = v_create_node(node);
    if (ignoreList == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_PlayerInviteFolder);
    uint32_t invites = v_create_node(node);
    if (invites == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_AgesIOwnFolder);
    uint32_t agesNode = v_create_node(node);
    if (agesNode == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_AgeJournalsFolder);
    uint32_t journals = v_create_node(node);
    if (journals == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_ChronicleFolder);
    uint32_t chronicles = v_create_node(node);
    if (chronicles == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeInfoList);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_AgesICanVisitFolder);
    uint32_t visitFolder = v_create_node(node);
    if (visitFolder == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_AvatarOutfitFolder);
    uint32_t outfit = v_create_node(node);
    if (outfit == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_AvatarClosetFolder);
    uint32_t closet = v_create_node(node);
    if (closet == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeFolder);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_InboxFolder);
    uint32_t inbox = v_create_node(node);
    if (inbox == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodePlayerInfoList);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Int32_1(DS::Vault::e_PeopleIKnowAboutFolder);
    uint32_t peopleNode = v_create_node(node);
    if (peopleNode == 0)
        return std::make_pair(0, 0);

    DS::Blob link(reinterpret_cast<const uint8_t*>("Default:LinkInPointDefault:;"),
                  strlen("Default:LinkInPointDefault:;"));
    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeLink);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Blob_1(link);
    uint32_t reltoLink = v_create_node(node);
    if (reltoLink == 0)
        return std::make_pair(0, 0);

    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeLink);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Blob_1(link);
    uint32_t hoodLink = v_create_node(node);
    if (hoodLink == 0)
        return std::make_pair(0, 0);

    link = DS::Blob(reinterpret_cast<const uint8_t*>("Ferry Terminal:LinkInPointFerry:;"),
                    strlen("Ferry Terminal:LinkInPointFerry:;"));
    node.clear();
    node.set_NodeType(DS::Vault::e_NodeAgeLink);
    node.set_CreatorUuid(acctId);
    node.set_CreatorIdx(playerIdx);
    node.set_Blob_1(link);
    uint32_t cityLink = v_create_node(node);
    if (hoodLink == 0)
        return std::make_pair(0, 0);

    AuthServer_AgeInfo relto;
    relto.m_ageId = gen_uuid();
    relto.m_filename = "Personal";
    relto.m_instName = "Relto";
    relto.m_userName = player.m_playerName + "'s";
    relto.m_description = relto.m_userName + " " + relto.m_instName;
    std::pair<uint32_t, uint32_t> reltoAge = v_create_age(relto, e_AgeIsRelto);
    if (reltoAge.first == 0)
        return std::make_pair(0, 0);

    {
        PostgresStrings<2> parms;
        parms.set(0, reltoAge.second);
        parms.set(1, DS::Vault::e_AgeOwnersFolder);
        PGresult* result = PQexecParams(s_postgres,
                "SELECT idx FROM vault.find_folder($1, $2);",
                2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return std::make_pair(0, 0);
        }
        DS_DASSERT(PQntuples(result) == 1);
        uint32_t ownerFolder = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);

        if (!v_ref_node(ownerFolder, playerInfoNode, 0))
            return std::make_pair(0, 0);
    }

    uint32_t hoodAge = find_a_friendly_neighborhood_for_our_new_visitor(playerInfoNode);
    if (hoodAge == 0)
        return std::make_pair(0, 0);
    uint32_t cityAge = find_public_age_1("city");
    if (cityAge == 0)
        return std::make_pair(0, 0);

    {
        PostgresStrings<2> parms;
        parms.set(0, reltoAge.first);
        parms.set(1, DS::Vault::e_AgesIOwnFolder);
        PGresult* result = PQexecParams(s_postgres,
                "SELECT idx FROM vault.find_folder($1, $2);",
                2, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return std::make_pair(0, 0);
        }
        DS_DASSERT(PQntuples(result) == 1);
        uint32_t bookshelfFolder = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);

        link = DS::Blob(reinterpret_cast<const uint8_t*>("Default:LinkInPointDefault:;"),
                        strlen("Default:LinkInPointDefault:;"));
        node.clear();
        node.set_NodeType(DS::Vault::e_NodeAgeLink);
        node.set_CreatorUuid(acctId);
        node.set_CreatorIdx(playerIdx);
        node.set_Blob_1(link);
        uint32_t reltoLinkBookshelf = v_create_node(node);
        if (reltoLinkBookshelf == 0)
            return std::make_pair(0, 0);

        node.clear();
        node.set_NodeType(DS::Vault::e_NodeAgeLink);
        node.set_CreatorUuid(acctId);
        node.set_CreatorIdx(playerIdx);
        node.set_Blob_1(link);
        uint32_t hoodLinkBookshelf = v_create_node(node);
        if (hoodLinkBookshelf == 0)
            return std::make_pair(0, 0);

        link = DS::Blob(reinterpret_cast<const uint8_t*>("Ferry Terminal:LinkInPointFerry:;"),
                        strlen("Ferry Terminal:LinkInPointFerry:;"));
        node.clear();
        node.set_NodeType(DS::Vault::e_NodeAgeLink);
        node.set_CreatorUuid(acctId);
        node.set_CreatorIdx(playerIdx);
        node.set_Blob_1(link);
        uint32_t cityLinkBookshelf = v_create_node(node);
        if (cityLinkBookshelf == 0)
            return std::make_pair(0, 0);

        if (!v_ref_node(bookshelfFolder, reltoLinkBookshelf, 0))
            return std::make_pair(0, 0);
        if (!v_ref_node(bookshelfFolder, hoodLinkBookshelf, 0))
            return std::make_pair(0, 0);
        if (!v_ref_node(bookshelfFolder, cityLinkBookshelf, 0))
            return std::make_pair(0, 0);
        if (!v_ref_node(reltoLinkBookshelf, reltoAge.second, 0))
            return std::make_pair(0, 0);
        if (!v_ref_node(hoodLinkBookshelf, hoodAge, 0))
            return std::make_pair(0, 0);
        if (!v_ref_node(cityLinkBookshelf, cityAge, 0))
            return std::make_pair(0, 0);
    }

    if (!v_ref_node(playerIdx, s_systemNode, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, playerInfoNode, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, buddyList, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, ignoreList, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, invites, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, agesNode, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, journals, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, chronicles, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, visitFolder, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, outfit, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, closet, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, inbox, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(playerIdx, peopleNode, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(agesNode, reltoLink, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(agesNode, hoodLink, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(agesNode, cityLink, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(reltoLink, reltoAge.second, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(hoodLink, hoodAge, 0))
        return std::make_pair(0, 0);
    if (!v_ref_node(cityLink, cityAge, 0))
        return std::make_pair(0, 0);

    return std::make_pair(playerIdx, playerInfoNode);
}

uint32_t v_create_node(const DS::Vault::Node& node)
{
    /* This should be plenty to store everything we need without a bunch
     * of dynamic reallocations
     */
    PostgresStrings<31> parms;
    char fieldbuf[1024];

    size_t parmcount = 0;
    char* fieldp = fieldbuf;

    #define SET_FIELD(name, value) \
        { \
            parms.set(parmcount++, value); \
            fieldp += sprintf(fieldp, "\"" #name "\","); \
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
        sprintf(fieldp, "$%Zu,", i+1);
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

bool v_update_node(const DS::Vault::Node& node)
{
    /* This should be plenty to store everything we need without a bunch
     * of dynamic reallocations
     */
    PostgresStrings<32> parms;
    char fieldbuf[1024];

    size_t parmcount = 1;
    char* fieldp = fieldbuf;

    #define SET_FIELD(name, value) \
        { \
            parms.set(parmcount++, value); \
            fieldp += sprintf(fieldp, "\"" #name "\"=$%Zu,", parmcount); \
        }
    int now = time(0);
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
    *(fieldp - 1) = 0;  // Get rid of the last comma
    DS::String queryStr = "UPDATE vault.\"Nodes\"\n    SET ";
    queryStr += fieldbuf;
    queryStr += "\n    WHERE idx=$1";
    parms.set(0, node.m_NodeIdx);

    check_postgres();
    PGresult* result = PQexecParams(s_postgres, queryStr.c_str(),
                                    parmcount, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return false;
    }
    PQclear(result);
    return true;
}

DS::Vault::Node v_fetch_node(uint32_t nodeIdx)
{
    PostgresStrings<1> parm;
    parm.set(0, nodeIdx);
    PGresult* result = PQexecParams(s_postgres,
        "SELECT idx, \"CreateTime\", \"ModifyTime\", \"CreateAgeName\","
        "    \"CreateAgeUuid\", \"CreatorUuid\", \"CreatorIdx\", \"NodeType\","
        "    \"Int32_1\", \"Int32_2\", \"Int32_3\", \"Int32_4\","
        "    \"Uint32_1\", \"Uint32_2\", \"Uint32_3\", \"Uint32_4\","
        "    \"Uuid_1\", \"Uuid_2\", \"Uuid_3\", \"Uuid_4\","
        "    \"String64_1\", \"String64_2\", \"String64_3\", \"String64_4\","
        "    \"String64_5\", \"String64_6\", \"IString64_1\", \"IString64_2\","
        "    \"Text_1\", \"Text_2\", \"Blob_1\", \"Blob_2\""
        "    FROM vault.\"Nodes\" WHERE idx=$1",
        1, 0, parm.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return DS::Vault::Node();
    }
    if (PQntuples(result) == 0) {
        PQclear(result);
        return DS::Vault::Node();
    }
    DS_DASSERT(PQntuples(result) == 1);

    DS::Vault::Node node;
    node.set_NodeIdx(strtoul(PQgetvalue(result, 0, 0), 0, 10));
    node.set_CreateTime(strtoul(PQgetvalue(result, 0, 1), 0, 10));
    node.set_ModifyTime(strtoul(PQgetvalue(result, 0, 2), 0, 10));
    if (!PQgetisnull(result, 0, 3))
        node.set_CreateAgeName(PQgetvalue(result, 0, 3));
    if (!PQgetisnull(result, 0, 4))
        node.set_CreateAgeUuid(PQgetvalue(result, 0, 4));
    if (!PQgetisnull(result, 0, 5))
        node.set_CreatorUuid(PQgetvalue(result, 0, 5));
    if (!PQgetisnull(result, 0, 6))
        node.set_CreatorIdx(strtoul(PQgetvalue(result, 0, 6), 0, 10));
    node.set_NodeType(strtoul(PQgetvalue(result, 0, 7), 0, 10));
    if (!PQgetisnull(result, 0, 8))
        node.set_Int32_1(strtol(PQgetvalue(result, 0, 8), 0, 10));
    if (!PQgetisnull(result, 0, 9))
        node.set_Int32_2(strtol(PQgetvalue(result, 0, 9), 0, 10));
    if (!PQgetisnull(result, 0, 10))
        node.set_Int32_3(strtol(PQgetvalue(result, 0, 10), 0, 10));
    if (!PQgetisnull(result, 0, 11))
        node.set_Int32_4(strtol(PQgetvalue(result, 0, 11), 0, 10));
    if (!PQgetisnull(result, 0, 12))
        node.set_Uint32_1(strtoul(PQgetvalue(result, 0, 12), 0, 10));
    if (!PQgetisnull(result, 0, 13))
        node.set_Uint32_2(strtoul(PQgetvalue(result, 0, 13), 0, 10));
    if (!PQgetisnull(result, 0, 14))
        node.set_Uint32_3(strtoul(PQgetvalue(result, 0, 14), 0, 10));
    if (!PQgetisnull(result, 0, 15))
        node.set_Uint32_4(strtoul(PQgetvalue(result, 0, 15), 0, 10));
    if (!PQgetisnull(result, 0, 16))
        node.set_Uuid_1(PQgetvalue(result, 0, 16));
    if (!PQgetisnull(result, 0, 17))
        node.set_Uuid_2(PQgetvalue(result, 0, 17));
    if (!PQgetisnull(result, 0, 18))
        node.set_Uuid_3(PQgetvalue(result, 0, 18));
    if (!PQgetisnull(result, 0, 19))
        node.set_Uuid_4(PQgetvalue(result, 0, 19));
    if (!PQgetisnull(result, 0, 20))
        node.set_String64_1(PQgetvalue(result, 0, 20));
    if (!PQgetisnull(result, 0, 21))
        node.set_String64_2(PQgetvalue(result, 0, 21));
    if (!PQgetisnull(result, 0, 22))
        node.set_String64_3(PQgetvalue(result, 0, 22));
    if (!PQgetisnull(result, 0, 23))
        node.set_String64_4(PQgetvalue(result, 0, 23));
    if (!PQgetisnull(result, 0, 24))
        node.set_String64_5(PQgetvalue(result, 0, 24));
    if (!PQgetisnull(result, 0, 25))
        node.set_String64_6(PQgetvalue(result, 0, 25));
    if (!PQgetisnull(result, 0, 26))
        node.set_IString64_1(PQgetvalue(result, 0, 26));
    if (!PQgetisnull(result, 0, 27))
        node.set_IString64_2(PQgetvalue(result, 0, 27));
    if (!PQgetisnull(result, 0, 28))
        node.set_Text_1(PQgetvalue(result, 0, 28));
    if (!PQgetisnull(result, 0, 29))
        node.set_Text_2(PQgetvalue(result, 0, 29));
    if (!PQgetisnull(result, 0, 30))
        node.set_Blob_1(DS::Base64Decode(PQgetvalue(result, 0, 30)));
    if (!PQgetisnull(result, 0, 31))
        node.set_Blob_2(DS::Base64Decode(PQgetvalue(result, 0, 31)));

    PQclear(result);
    return node;
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

bool v_unref_node(uint32_t parentIdx, uint32_t childIdx)
{
    PostgresStrings<2> parms;
    parms.set(0, parentIdx);
    parms.set(1, childIdx);
    PGresult* result = PQexecParams(s_postgres,
            "DELETE FROM vault.\"NodeRefs\""
            "    WHERE \"ParentIdx\"=$1 AND \"ChildIdx\"=$2",
            2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres DELETE error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return false;
    }
    PQclear(result);
    return true;
}

bool v_fetch_tree(uint32_t nodeId, std::vector<DS::Vault::NodeRef>& refs)
{
    PostgresStrings<1> parm;
    parm.set(0, nodeId);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT \"ParentIdx\", \"ChildIdx\", \"OwnerIdx\", \"Seen\""
            "    FROM vault.fetch_tree($1);",
            1, 0, parm.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return false;
    }

    refs.resize(PQntuples(result));
    for (size_t i=0; i<refs.size(); ++i) {
        refs[i].m_parent = strtoul(PQgetvalue(result, i, 0), 0, 10);
        refs[i].m_child = strtoul(PQgetvalue(result, i, 1), 0, 10);
        refs[i].m_owner = strtoul(PQgetvalue(result, i, 2), 0, 10);
        refs[i].m_seen = strtoul(PQgetvalue(result, i, 3), 0, 10);
    }
    PQclear(result);
    return true;
}

bool v_find_nodes(const DS::Vault::Node& nodeTemplate, std::vector<uint32_t>& nodes)
{
    if (nodeTemplate.isNull())
        return false;

    /* This should be plenty to store everything we need without a bunch
     * of dynamic reallocations
     */
    PostgresStrings<31> parms;
    char fieldbuf[1024];

    size_t parmcount = 0;
    char* fieldp = fieldbuf;

    #define SET_FIELD(name, value) \
        { \
            parms.set(parmcount++, value); \
            fieldp += sprintf(fieldp, "\"" #name "\"=$%Zu AND ", parmcount); \
        }
    #define SET_FIELD_I(name, value) \
        { \
            parms.set(parmcount++, value); \
            fieldp += sprintf(fieldp, "LOWER(\"" #name "\")=LOWER($%Zu) AND ", parmcount); \
        }
    if (nodeTemplate.has_CreateTime())
        SET_FIELD(CreateTime, nodeTemplate.m_CreateTime);
    if (nodeTemplate.has_ModifyTime())
        SET_FIELD(ModifyTime, nodeTemplate.m_ModifyTime);
    if (nodeTemplate.has_CreateAgeName())
        SET_FIELD(CreateAgeName, nodeTemplate.m_CreateAgeName);
    if (nodeTemplate.has_CreateAgeUuid())
        SET_FIELD(CreateAgeUuid, nodeTemplate.m_CreateAgeUuid.toString());
    if (nodeTemplate.has_CreatorUuid())
        SET_FIELD(CreatorUuid, nodeTemplate.m_CreatorUuid.toString());
    if (nodeTemplate.has_CreatorIdx())
        SET_FIELD(CreatorIdx, nodeTemplate.m_CreatorIdx);
    if (nodeTemplate.has_NodeType())
        SET_FIELD(NodeType, nodeTemplate.m_NodeType);
    if (nodeTemplate.has_Int32_1())
        SET_FIELD(Int32_1, nodeTemplate.m_Int32_1);
    if (nodeTemplate.has_Int32_2())
        SET_FIELD(Int32_2, nodeTemplate.m_Int32_2);
    if (nodeTemplate.has_Int32_3())
        SET_FIELD(Int32_3, nodeTemplate.m_Int32_3);
    if (nodeTemplate.has_Int32_4())
        SET_FIELD(Int32_4, nodeTemplate.m_Int32_4);
    if (nodeTemplate.has_Uint32_1())
        SET_FIELD(Uint32_1, nodeTemplate.m_Uint32_1);
    if (nodeTemplate.has_Uint32_2())
        SET_FIELD(Uint32_2, nodeTemplate.m_Uint32_2);
    if (nodeTemplate.has_Uint32_3())
        SET_FIELD(Uint32_3, nodeTemplate.m_Uint32_3);
    if (nodeTemplate.has_Uint32_4())
        SET_FIELD(Uint32_4, nodeTemplate.m_Uint32_4);
    if (nodeTemplate.has_Uuid_1())
        SET_FIELD(Uuid_1, nodeTemplate.m_Uuid_1.toString());
    if (nodeTemplate.has_Uuid_2())
        SET_FIELD(Uuid_2, nodeTemplate.m_Uuid_2.toString());
    if (nodeTemplate.has_Uuid_3())
        SET_FIELD(Uuid_3, nodeTemplate.m_Uuid_3.toString());
    if (nodeTemplate.has_Uuid_4())
        SET_FIELD(Uuid_4, nodeTemplate.m_Uuid_4.toString());
    if (nodeTemplate.has_String64_1())
        SET_FIELD(String64_1, nodeTemplate.m_String64_1);
    if (nodeTemplate.has_String64_2())
        SET_FIELD(String64_2, nodeTemplate.m_String64_2);
    if (nodeTemplate.has_String64_3())
        SET_FIELD(String64_3, nodeTemplate.m_String64_3);
    if (nodeTemplate.has_String64_4())
        SET_FIELD(String64_4, nodeTemplate.m_String64_4);
    if (nodeTemplate.has_String64_5())
        SET_FIELD(String64_5, nodeTemplate.m_String64_5);
    if (nodeTemplate.has_String64_6())
        SET_FIELD(String64_6, nodeTemplate.m_String64_6);
    if (nodeTemplate.has_IString64_1())
        SET_FIELD_I(IString64_1, nodeTemplate.m_IString64_1);
    if (nodeTemplate.has_IString64_2())
        SET_FIELD_I(IString64_2, nodeTemplate.m_IString64_2);
    if (nodeTemplate.has_Text_1())
        SET_FIELD(Text_1, nodeTemplate.m_Text_1);
    if (nodeTemplate.has_Text_2())
        SET_FIELD(Text_2, nodeTemplate.m_Text_2);
    if (nodeTemplate.has_Blob_1())
        SET_FIELD(Blob_1, DS::Base64Encode(nodeTemplate.m_Blob_1.buffer(), nodeTemplate.m_Blob_1.size()));
    if (nodeTemplate.has_Blob_2())
        SET_FIELD(Blob_2, DS::Base64Encode(nodeTemplate.m_Blob_2.buffer(), nodeTemplate.m_Blob_2.size()));
    #undef SET_FIELD
    #undef SET_FIELD_I

    DS_DASSERT(parmcount > 0);
    DS_DASSERT(fieldp - fieldbuf < 1024);
    *(fieldp - 5) = 0;  // Get rid of the last ' AND '
    DS::String queryStr = "SELECT idx FROM vault.\"Nodes\"\n    WHERE ";
    queryStr += fieldbuf;

    check_postgres();
    PGresult* result = PQexecParams(s_postgres, queryStr.c_str(),
                                    parmcount, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return false;
    }

    nodes.resize(PQntuples(result));
    for (size_t i=0; i<nodes.size(); ++i)
        nodes[i] = strtoul(PQgetvalue(result, i, 0), 0, 10);
    PQclear(result);
    return true;
}

bool v_send_node(uint32_t nodeId, uint32_t playerId)
{
    PostgresStrings<2> parms;
    parms.set(0, playerId);
    parms.set(1, DS::Vault::e_InboxFolder);
    PGresult* result = PQexecParams(s_postgres,
            "SELECT idx FROM vault.find_folder($1, $2);",
            2, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(s_postgres));
        PQclear(result);
        return false;
    }
    DS_DASSERT(PQntuples(result) == 1);
    uint32_t inbox = strtoul(PQgetvalue(result, 0, 0), 0, 10);
    PQclear(result);

    if (!v_ref_node(inbox, nodeId, 0))
        return false;
    return true;
}
