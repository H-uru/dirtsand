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
                      DS::String userName, bool publicAge)
{
    //TODO
}

uint32_t v_create_player(DS::Uuid playerId, DS::String playerName,
                         DS::String avatarShape)
{
    //TODO
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
