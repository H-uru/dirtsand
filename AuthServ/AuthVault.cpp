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
        int now = time(0);
        {
            PostgresStrings<3> iparms;
            iparms.set(0, DS::Vault::e_NodeSystem);
            iparms.set(1, now);
            iparms.set(2, now);
            result = PQexecParams(s_postgres,
                "INSERT INTO vault.\"Nodes\""
                "    (\"NodeType\", \"CreateTime\", \"ModifyTime\")"
                "    VALUES ($1, $2, $3)"
                "    RETURNING idx",
                3, 0, iparms.m_values, 0, 0, 0);
        }
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return false;
        }
        DS_DASSERT(PQntuples(result) == 1);
        s_systemNode = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);

        {
            PostgresStrings<4> iparms;
            iparms.set(0, DS::Vault::e_NodeFolder);
            iparms.set(1, now);
            iparms.set(2, now);
            iparms.set(3, DS::Vault::e_GlobalInboxFolder);
            result = PQexecParams(s_postgres,
                "INSERT INTO vault.\"Nodes\""
                "    (\"NodeType\", \"CreateTime\", \"ModifyTime\", \"Int32_1\")"
                "    VALUES ($1, $2, $3, $4)"
                "    RETURNING idx",
                4, 0, iparms.m_values, 0, 0, 0);
        }
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return false;
        }
        DS_DASSERT(PQntuples(result) == 1);
        uint32_t globalInbox = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);

        {
            PostgresStrings<2> iparms;
            iparms.set(0, s_systemNode);
            iparms.set(1, globalInbox);
            result = PQexecParams(s_postgres,
                "INSERT INTO vault.\"NodeRefs\""
                "    (\"ParentIdx\", \"ChildIdx\")"
                "    VALUES ($1, $2)",
                2, 0, iparms.m_values, 0, 0, 0);
        }
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(s_postgres));
            PQclear(result);
            return false;
        }
        PQclear(result);
    } else {
        DS_DASSERT(count == 1);
        s_systemNode = strtoul(PQgetvalue(result, 0, 0), 0, 10);
        PQclear(result);
    }

    return true;
}
