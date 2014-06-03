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
#include "AuthClient.h"
#include "db/pqaccess.h"
#include "SDL/StateInfo.h"
#include "streams.h"
#include <vector>
#include <list>
#include <map>
#include <thread>
#include <mutex>

enum AuthServer_MsgIds
{
    e_CliToAuth_PingRequest = 0, e_CliToAuth_ClientRegisterRequest,
    e_CliToAuth_ClientSetCCRLevel, e_CliToAuth_AcctLoginRequest,
    e_CliToAuth_AcctSetEulaVersion, e_CliToAuth_AcctSetDataRequest,
    e_CliToAuth_AcctSetPlayerRequest, e_CliToAuth_AcctCreateRequest,
    e_CliToAuth_AcctChangePasswordRequest, e_CliToAuth_AcctSetRolesRequest,
    e_CliToAuth_AcctSetBillingTypeRequest, e_CliToAuth_AcctActivateRequest,
    e_CliToAuth_AcctCreateFromKeyRequest, e_CliToAuth_PlayerDeleteRequest,
    e_CliToAuth_PlayerUndeleteRequest, e_CliToAuth_PlayerSelectRequest,
    e_CliToAuth_PlayerRenameRequest, e_CliToAuth_PlayerCreateRequest,
    e_CliToAuth_PlayerSetStatus, e_CliToAuth_PlayerChat,
    e_CliToAuth_UpgradeVisitorRequest, e_CliToAuth_SetPlayerBanStatusRequest,
    e_CliToAuth_KickPlayer, e_CliToAuth_ChangePlayerNameRequest,
    e_CliToAuth_SendFriendInviteRequest, e_CliToAuth_VaultNodeCreate,
    e_CliToAuth_VaultNodeFetch, e_CliToAuth_VaultNodeSave,
    e_CliToAuth_VaultNodeDelete, e_CliToAuth_VaultNodeAdd,
    e_CliToAuth_VaultNodeRemove, e_CliToAuth_VaultFetchNodeRefs,
    e_CliToAuth_VaultInitAgeRequest, e_CliToAuth_VaultNodeFind,
    e_CliToAuth_VaultSetSeen, e_CliToAuth_VaultSendNode, e_CliToAuth_AgeRequest,
    e_CliToAuth_FileListRequest, e_CliToAuth_FileDownloadRequest,
    e_CliToAuth_FileDownloadChunkAck, e_CliToAuth_PropagateBuffer,
    e_CliToAuth_GetPublicAgeList, e_CliToAuth_SetAgePublic,
    e_CliToAuth_LogPythonTraceback, e_CliToAuth_LogStackDump,
    e_CliToAuth_LogClientDebuggerConnect, e_CliToAuth_ScoreCreate,
    e_CliToAuth_ScoreDelete, e_CliToAuth_ScoreGetScores,
    e_CliToAuth_ScoreAddPoints, e_CliToAuth_ScoreTransferPoints,
    e_CliToAuth_ScoreSetPoints, e_CliToAuth_ScoreGetRanks,
    e_CliToAuth_AcctExistsRequest,
    e_CliToAuth_AgeRequestEx = 0x1000,

    e_AuthToCli_PingReply = 0, kAuthToCli_ServerAddr, e_AuthToCli_NotifyNewBuild,
    e_AuthToCli_ClientRegisterReply, e_AuthToCli_AcctLoginReply,
    e_AuthToCli_AcctData, e_AuthToCli_AcctPlayerInfo,
    e_AuthToCli_AcctSetPlayerReply, e_AuthToCli_AcctCreateReply,
    e_AuthToCli_AcctChangePasswordReply, e_AuthToCli_AcctSetRolesReply,
    e_AuthToCli_AcctSetBillingTypeReply, e_AuthToCli_AcctActivateReply,
    e_AuthToCli_AcctCreateFromKeyReply, e_AuthToCli_PlayerList,
    e_AuthToCli_PlayerChat, e_AuthToCli_PlayerCreateReply,
    e_AuthToCli_PlayerDeleteReply, e_AuthToCli_UpgradeVisitorReply,
    e_AuthToCli_SetPlayerBanStatusReply, e_AuthToCli_ChangePlayerNameReply,
    e_AuthToCli_SendFriendInviteReply, e_AuthToCli_FriendNotify,
    e_AuthToCli_VaultNodeCreated, e_AuthToCli_VaultNodeFetched,
    e_AuthToCli_VaultNodeChanged, e_AuthToCli_VaultNodeDeleted,
    e_AuthToCli_VaultNodeAdded, e_AuthToCli_VaultNodeRemoved,
    e_AuthToCli_VaultNodeRefsFetched, e_AuthToCli_VaultInitAgeReply,
    e_AuthToCli_VaultNodeFindReply, e_AuthToCli_VaultSaveNodeReply,
    e_AuthToCli_VaultAddNodeReply, e_AuthToCli_VaultRemoveNodeReply,
    e_AuthToCli_AgeReply, e_AuthToCli_FileListReply,
    e_AuthToCli_FileDownloadChunk, e_AuthToCli_PropagateBuffer,
    e_AuthToCli_KickedOff, e_AuthToCli_PublicAgeList,
    e_AuthToCli_ScoreCreateReply, e_AuthToCli_ScoreDeleteReply,
    e_AuthToCli_ScoreGetScoresReply, e_AuthToCli_ScoreAddPointsReply,
    e_AuthToCli_ScoreTransferPointsReply, e_AuthToCli_ScoreSetPointsReply,
    e_AuthToCli_ScoreGetRanksReply, e_AuthToCli_AcctExistsReply,
    e_AuthToCli_AgeReplyEx = 0x1000,
};

struct AuthServer_Private : public AuthClient_Private
{
    DS::BufferStream m_buffer;
    uint32_t m_serverChallenge;
    DS::Uuid m_acctUuid;
    uint32_t m_acctFlags;
    AuthServer_PlayerInfo m_player;
    uint32_t m_ageNodeId;
    std::map<uint32_t, DS::Stream*> m_downloads;

    AuthServer_Private() : m_serverChallenge(0), m_acctFlags(0), m_ageNodeId(0) { }

    ~AuthServer_Private()
    {
        while (!m_downloads.empty()) {
            auto item = m_downloads.begin();
            delete item->second;
            m_downloads.erase(item);
        }
    }
};

extern std::list<AuthServer_Private*> s_authClients;
extern std::mutex s_authClientMutex;
extern std::thread s_authDaemonThread;

void dm_authDaemon();
bool dm_vault_init();
bool dm_global_sdl_init();
bool dm_check_static_ages();
bool dm_all_players_init();

DS::Uuid gen_uuid();

enum AgeFlags
{
    e_AgePublic  = (1<<0),
};

bool v_check_global_sdl(const DS::String& name, SDL::StateDescriptor* desc);
SDL::State v_find_global_sdl(const DS::String& ageName);

std::tuple<uint32_t, uint32_t>
v_create_age(AuthServer_AgeInfo age, uint32_t flags);

std::tuple<uint32_t, uint32_t, uint32_t> // playerId, playerInfoId, hoodAgeOwnersFolderId
v_create_player(DS::Uuid accountId, const AuthServer_PlayerInfo& player);

uint32_t v_create_node(const DS::Vault::Node& node);
bool v_update_node(const DS::Vault::Node& node);
DS::Vault::Node v_fetch_node(uint32_t nodeIdx);
bool v_has_node(uint32_t parentId, uint32_t childId);
bool v_ref_node(uint32_t parentIdx, uint32_t childIdx, uint32_t ownerIdx);
bool v_unref_node(uint32_t parentIdx, uint32_t childIdx);
bool v_fetch_tree(uint32_t nodeId, std::vector<DS::Vault::NodeRef>& refs);
bool v_find_nodes(const DS::Vault::Node& nodeTemplate, std::vector<uint32_t>& nodes);
DS::Vault::NodeRef v_send_node(uint32_t nodeId, uint32_t playerId, uint32_t senderId);

uint32_t v_count_age_owners(uint32_t ageInfoId);
bool v_find_public_ages(const DS::String& ageFilename, std::vector<Auth_PubAgeRequest::NetAgeInfo>& ages);
