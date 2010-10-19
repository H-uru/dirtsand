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
#include "NetIO/CryptIO.h"
#include "NetIO/MsgChannel.h"
#include "Types/Uuid.h"
#include "Types/ShaHash.h"
#include "streams.h"
#include <pthread.h>
#include <vector>
#include <list>

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
    AuthServer_PlayerInfo m_player;
};

extern std::list<AuthServer_Private*> s_authClients;
extern pthread_mutex_t s_authClientMutex;
extern pthread_t s_authDaemonThread;
extern DS::MsgChannel s_authChannel;

void* dm_authDaemon(void*);
bool dm_auth_init();

enum AuthDaemonMessages
{
    e_AuthShutdown, e_AuthClientLogin,
};
void AuthDaemon_SendMessage(int msg, void* data = 0);

struct Auth_ClientMessage
{
    AuthServer_Private* m_client;
    uint32_t m_transId;
};

struct Auth_LoginInfo : public Auth_ClientMessage
{
    uint32_t m_clientChallenge;
    DS::String m_acctName;
    DS::ShaHash m_passHash;
    DS::String m_token, m_os;

    DS::Uuid m_acctUuid;
    uint32_t m_acctFlags, m_billingType;
    std::vector<AuthServer_PlayerInfo> m_players;
};
