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
};


extern DS::MsgChannel s_authChannel;

enum AuthDaemonMessages
{
    e_AuthShutdown, e_AuthClientLogin, e_AuthSetPlayer, e_AuthCreatePlayer,
    e_VaultCreateNode, e_VaultFetchNode, e_VaultUpdateNode, e_VaultRefNode,
    e_VaultUnrefNode, e_VaultFetchNodeTree, e_VaultFindNode, e_VaultInitAge,
    e_AuthFindGameServer, e_AuthDisconnect, e_AuthAddAcct, e_AuthGetPublic,
    e_AuthSetPublic
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

    uint32_t m_acctFlags, m_billingType;
    std::vector<AuthServer_PlayerInfo> m_players;
};

struct Auth_PlayerCreate : public Auth_ClientMessage
{
    AuthServer_PlayerInfo m_player;
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
};

struct Auth_NodeRef : public Auth_ClientMessage
{
    DS::Vault::NodeRef m_ref;
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
    struct pnNetAgeInfo
    {
        enum { Stride = 0x9A0 };

        DS::Uuid m_instance;
        DS::String m_instancename, m_username, m_description;
        uint32_t m_sequence, m_language, m_population;
    };

    DS::String m_agename;
    std::vector<pnNetAgeInfo> m_ages;
};

struct Auth_SetPublic : public Auth_ClientMessage
{
    uint32_t m_node;
    uint8_t m_public;
};

DS::Blob gen_default_sdl(const DS::String& filename);
