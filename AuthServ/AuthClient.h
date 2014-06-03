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

#include "AuthServer.h"
#include "VaultTypes.h"
#include "SDL/StateInfo.h"
#include "NetIO/SockIO.h"
#include "NetIO/CryptIO.h"
#include "NetIO/MsgChannel.h"
#include "Types/Uuid.h"
#include "Types/ShaHash.h"

struct AuthClient_Private
{
    DS::SocketHandle m_sock;
    DS::CryptState m_crypt;
    DS::MsgChannel m_channel;
};

struct AuthServer_PlayerInfo
{
    uint32_t m_playerId;
    DS::String m_playerName;
    DS::String m_avatarModel;
    uint32_t m_explorer;

    AuthServer_PlayerInfo() : m_playerId(0), m_explorer(1) { }
};

struct AuthServer_AgeInfo
{
    DS::Uuid m_ageId, m_parentId;
    DS::String m_filename, m_instName, m_userName;
    DS::String m_description;
    int m_seqNumber, m_language;

    AuthServer_AgeInfo() : m_seqNumber(0), m_language(-1) { }

    void clear()
    {
        m_ageId = DS::Uuid();
        m_parentId = DS::Uuid();
        m_filename = DS::String();
        m_instName = DS::String();
        m_userName = DS::String();
        m_description = DS::String();
        m_seqNumber = 0;
        m_language = -1;
    }
};


extern DS::MsgChannel s_authChannel;

enum AuthDaemonMessages
{
    e_AuthShutdown, e_AuthClientLogin, e_AuthSetPlayer, e_AuthCreatePlayer,
    e_AuthDeletePlayer, e_VaultCreateNode, e_VaultFetchNode, e_VaultUpdateNode,
    e_VaultRefNode, e_VaultUnrefNode, e_VaultFetchNodeTree, e_VaultFindNode,
    e_VaultSendNode, e_VaultInitAge,  e_AuthFindGameServer, e_AuthDisconnect,
    e_AuthAddAcct, e_AuthGetPublic, e_AuthSetPublic, e_AuthCreateScore,
    e_AuthGetScores, e_AuthAddScorePoints, e_AuthTransferScorePoints,
    e_AuthUpdateAgeSrv, e_AuthAcctFlags, e_AuthRestrictLogins, e_AuthAddAllPlayers,
    e_AuthFetchSDL, e_AuthUpdateGlobalSDL
};

struct Auth_AccountInfo
{
    DS::String m_acctName;
    DS::String m_password;
};

struct Auth_ClientMessage
{
    AuthClient_Private* m_client;
};

struct Auth_LoginInfo : public Auth_ClientMessage
{
    uint32_t m_clientChallenge;
    DS::String m_acctName;
    DS::ShaHash m_passHash;
    DS::String m_token, m_os;

    uint32_t m_billingType;
    std::vector<AuthServer_PlayerInfo> m_players;
};

struct Auth_PlayerCreate : public Auth_ClientMessage
{
    AuthServer_PlayerInfo m_player;
};

struct Auth_PlayerDelete : public Auth_ClientMessage
{
    uint32_t m_playerId;
};

struct Auth_AgeCreate : public Auth_ClientMessage
{
    AuthServer_AgeInfo m_age;
    uint32_t m_ageIdx, m_infoIdx;
};

struct Auth_NodeInfo : public Auth_ClientMessage
{
    DS::Vault::Node m_node;
    DS::Uuid m_revision;
    bool m_internal;
};

struct Auth_NodeRef : public Auth_ClientMessage
{
    DS::Vault::NodeRef m_ref;
};

struct Auth_NodeSend : public Auth_ClientMessage
{
    uint32_t m_nodeIdx;
    uint32_t m_playerIdx;
    uint32_t m_senderIdx;
};

struct Auth_NodeRefList : public Auth_ClientMessage
{
    uint32_t m_nodeId;
    std::vector<DS::Vault::NodeRef> m_refs;
};

struct Auth_NodeFindList : public Auth_ClientMessage
{
    DS::Vault::Node m_template;
    std::vector<uint32_t> m_nodes;
};

struct Auth_GameAge : public Auth_ClientMessage
{
    DS::String m_name;
    DS::Uuid m_instanceId;

    uint32_t m_mcpId;
    uint32_t m_ageNodeIdx;
    uint32_t m_serverAddress;
};

struct Auth_PubAgeRequest : public Auth_ClientMessage
{
    struct NetAgeInfo
    {
        enum { Stride = 0x9A0 };

        DS::Uuid m_instance;
        DS::String m_instancename, m_username, m_description;
        uint32_t m_sequence, m_language, m_curPopulation, m_population;
    };

    DS::String m_agename;
    std::vector<NetAgeInfo> m_ages;
};

struct Auth_SetPublic : public Auth_ClientMessage
{
    uint32_t m_node;
    uint8_t m_public;
};

struct Auth_CreateScore : public Auth_ClientMessage
{
    uint32_t m_scoreId;
    uint32_t m_owner;
    DS::String m_name;
    uint32_t m_type;
    int32_t m_points;
};

struct Auth_GetScores : public Auth_ClientMessage
{
    struct GameScore
    {
        // Plus number of bytes in the name!
        enum { BaseStride = 0x18 };

        uint32_t m_scoreId;
        uint32_t m_owner;
        uint32_t m_createTime;
        uint32_t m_type;
        int32_t m_points;
    };

    uint32_t m_owner;
    DS::String m_name;
    std::vector<GameScore> m_scores; // there should only ever be one in DS
};

struct Auth_UpdateScore : public Auth_ClientMessage
{
    enum { e_Fixed, e_Football, e_Golf };
    uint32_t m_scoreId;
    int32_t m_points;
};

struct Auth_TransferScore : public Auth_ClientMessage
{
    uint32_t m_srcScoreId, m_dstScoreId;
    uint32_t m_points;
};

struct Auth_UpdateAgeSrv : public Auth_ClientMessage
{
    uint32_t m_playerId;
    uint32_t m_ageNodeId;
    bool m_isAdmin;
};

struct Auth_AccountFlags : public Auth_ClientMessage
{
    DS::String m_acctName;
    uint32_t m_flags;
};

struct Auth_AddAcct : public Auth_ClientMessage
{
    Auth_AccountInfo m_acctInfo;
};

struct Auth_RestrictLogins : public Auth_ClientMessage
{
    bool m_status;
};

struct Auth_AddAllPlayers : public Auth_ClientMessage
{
    uint32_t m_playerId;
};

struct Auth_FetchSDL : public Auth_ClientMessage
{
    DS::String m_ageFilename;
    uint32_t m_sdlNodeId;

    DS::Blob m_localState;
    SDL::State m_globalState;
};

struct Auth_UpdateGlobalSDL : public Auth_ClientMessage
{
    DS::String m_ageFilename;
    DS::String m_variable;
    DS::String m_value;
};

DS::Blob gen_default_sdl(const DS::String& filename);
