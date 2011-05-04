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

#include "GameServer.h"
#include "AuthServ/AuthClient.h"
#include "NetIO/CryptIO.h"
#include "NetIO/MsgChannel.h"
#include "Types/Uuid.h"
#include "PlasMOUL/factory.h"
#include "PlasMOUL/NetMessages/NetMsgMembersList.h"
#include "SDL/StateInfo.h"
#include "db/pqaccess.h"
#include <unordered_map>
#include <unordered_set>
#include <list>

enum GameServer_MsgIds
{
    e_CliToGame_PingRequest = 0, e_CliToGame_JoinAgeRequest,
    e_CliToGame_Propagatebuffer, e_CliToGame_GameMgrMsg,

    e_GameToCli_PingReply = 0, e_GameToCli_JoinAgeReply,
    e_GameToCli_PropagateBuffer, e_GameToCli_GameMgrMsg,
};

struct GameClient_Private : public AuthClient_Private
{
    struct GameHost_Private* m_host;
    DS::BufferStream m_buffer;

    DS::Uuid m_clientId;
    MOUL::ClientGuid m_clientInfo;
    MOUL::Uoid m_clientKey;
};

typedef std::unordered_map<DS::String, SDL::State, DS::StringHash> sdlnamemap_t;
typedef std::unordered_map<MOUL::Uoid, sdlnamemap_t, MOUL::UoidHash> sdlstatemap_t;
typedef std::unordered_set<MOUL::Uoid, MOUL::UoidHash> uoidset_t;

struct GameHost_Private
{
    DS::Uuid m_instanceId;
    DS::String m_ageFilename;
    uint32_t m_ageIdx, m_serverIdx;

    std::unordered_map<uint32_t, GameClient_Private*> m_clients;
    uoidset_t m_locks;
    pthread_mutex_t m_clientMutex;
    pthread_mutex_t m_lockMutex;
    DS::MsgChannel m_channel;
    DS::BufferStream m_buffer;

    PGconn* m_postgres;
    uint32_t m_sdlIdx;
    SDL::State m_vaultState;
    sdlstatemap_t m_states;
};

typedef std::unordered_map<uint32_t, GameHost_Private*> hostmap_t;
extern hostmap_t s_gameHosts;
extern pthread_mutex_t s_gameHostMutex;

struct Game_AgeInfo
{
    uint32_t m_startTime, m_lingerTime;
    double m_dayLength;
    uint32_t m_maxCapacity;
    int32_t m_seqPrefix;

    Game_AgeInfo()
        : m_startTime(0), m_lingerTime(180), m_dayLength(24), m_maxCapacity(10),
          m_seqPrefix(0) { }
};
typedef std::unordered_map<DS::String, Game_AgeInfo, DS::StringHash> agemap_t;
extern agemap_t s_ages;

enum GameHostMessages
{
    e_GameShutdown, e_GameDisconnect, e_GameJoinAge, e_GamePropagate,
};

struct Game_ClientMessage
{
    GameClient_Private* m_client;
};

struct Game_PropagateMessage : public Game_ClientMessage
{
    uint32_t m_messageType;
    DS::Blob m_message;
};

GameHost_Private* start_game_host(uint32_t ageMcpId);
