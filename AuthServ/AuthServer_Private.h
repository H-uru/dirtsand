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

#include "AuthServer.h"
#include "VaultTypes.h"
#include "NetIO/CryptIO.h"
#include "NetIO/MsgChannel.h"
#include "Types/Uuid.h"
#include "Types/ShaHash.h"
#include "streams.h"
#include <libpq-fe.h>
#include <pthread.h>
#include <vector>
#include <list>
#include <map>

struct AuthServer_PlayerInfo
{
    uint32_t m_playerId;
    DS::String m_playerName;
    DS::String m_avatarModel;
    uint32_t m_explorer;
};

struct AuthServer_Private
{
    DS::SocketHandle m_sock;
    DS::CryptState m_crypt;
    DS::BufferStream m_buffer;
    DS::MsgChannel m_channel;

    uint32_t m_serverChallenge;
    DS::Uuid m_acctUuid;
    AuthServer_PlayerInfo m_player;
    std::map<uint32_t, DS::Stream*> m_downloads;

    ~AuthServer_Private()
    {
        while (!m_downloads.empty()) {
            std::map<uint32_t, DS::Stream*>::iterator item = m_downloads.begin();
            delete item->second;
            m_downloads.erase(item);
        }
    }
};

extern std::list<AuthServer_Private*> s_authClients;
extern pthread_mutex_t s_authClientMutex;
extern pthread_t s_authDaemonThread;
extern DS::MsgChannel s_authChannel;

void* dm_authDaemon(void*);
bool dm_auth_init();

enum AuthDaemonMessages
{
    e_AuthShutdown, e_AuthClientLogin, e_AuthSetPlayer, e_AuthCreatePlayer,
};
void AuthDaemon_SendMessage(int msg, void* data = 0);

struct Auth_ClientMessage
{
    AuthServer_Private* m_client;
};

struct Auth_LoginInfo : public Auth_ClientMessage
{
    uint32_t m_clientChallenge;
    DS::String m_acctName;
    DS::ShaHash m_passHash;
    DS::String m_token, m_os;

    uint32_t m_acctFlags, m_billingType;
    std::vector<AuthServer_PlayerInfo> m_players;
};

struct Auth_PlayerCreate : public Auth_ClientMessage
{
    DS::String m_playerName;
    DS::String m_avatarShape;

    uint32_t m_playerNode;
};

/* Vault/Postgres stuff */
template <size_t count>
struct PostgresStrings
{
    const char* m_values[count];
    DS::String m_strings[count];

    void set(size_t idx, const DS::String& str)
    {
        m_strings[idx] = str;
        this->m_values[idx] = str.c_str();
    }

    void set(size_t idx, uint32_t value)
    {
        m_strings[idx] = DS::String::Format("%u", value);
        this->m_values[idx] = m_strings[idx].c_str();
    }

    void set(size_t idx, int value)
    {
        m_strings[idx] = DS::String::Format("%d", value);
        this->m_values[idx] = m_strings[idx].c_str();
    }
};

extern PGconn* s_postgres;
DS::Uuid gen_uuid();

bool init_vault();

std::pair<uint32_t, uint32_t>
v_create_age(DS::Uuid ageId, DS::String filename, DS::String instName,
             DS::String userName, int32_t seqNumber, bool publicAge);

std::pair<uint32_t, uint32_t>
v_create_player(DS::Uuid playerId, DS::String playerName,
                DS::String avatarShape, bool explorer);

uint32_t v_create_node(const DS::Vault::Node& node);
bool v_ref_node(uint32_t parentIdx, uint32_t childIdx, uint32_t ownerIdx);
