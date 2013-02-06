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

#include "GameServer_Private.h"
#include "PlasMOUL/NetMessages/NetMsgMembersList.h"
#include "PlasMOUL/NetMessages/NetMsgLoadClone.h"
#include "PlasMOUL/NetMessages/NetMsgGameState.h"
#include "PlasMOUL/NetMessages/NetMsgSharedState.h"
#include "PlasMOUL/NetMessages/NetMsgSDLState.h"
#include "PlasMOUL/NetMessages/NetMsgGroupOwner.h"
#include "PlasMOUL/NetMessages/NetMsgVoice.h"
#include "PlasMOUL/Messages/ServerReplyMsg.h"
#include "PlasMOUL/Messages/LoadAvatarMsg.h"
#include "SDL/DescriptorDb.h"
#include "settings.h"
#include "errors.h"
#include "encodings.h"

hostmap_t s_gameHosts;
std::mutex s_gameHostMutex;
agemap_t s_ages;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

#define DM_WRITEMSG(host, msg) \
    host->m_buffer.truncate(); \
    host->m_buffer.write<uint16_t>(e_GameToCli_PropagateBuffer); \
    host->m_buffer.write<uint32_t>(msg->type()); \
    host->m_buffer.write<uint32_t>(0); \
    MOUL::Factory::WriteCreatable(&host->m_buffer, msg); \
    host->m_buffer.seek(6, SEEK_SET); \
    host->m_buffer.write<uint32_t>(host->m_buffer.size() - 10)

static inline void check_postgres(GameHost_Private* host)
{
    if (PQstatus(host->m_postgres) == CONNECTION_BAD)
        PQreset(host->m_postgres);
    DS_DASSERT(PQstatus(host->m_postgres) == CONNECTION_OK);
}

void dm_game_shutdown(GameHost_Private* host)
{
    {
        std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
        for (auto client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter)
            DS::CloseSock(client_iter->second->m_sock);
    }

    host->m_cloneMutex.lock();
    for (auto clone_iter = host->m_clones.begin(); clone_iter != host->m_clones.end(); ++clone_iter)
        clone_iter->second->unref();
    host->m_cloneMutex.unlock();

    bool complete = false;
    for (int i=0; i<50 && !complete; ++i) {
        host->m_clientMutex.lock();
        size_t alive = host->m_clients.size();
        host->m_clientMutex.unlock();
        if (alive == 0)
            complete = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!complete)
        fputs("[Game] Clients didn't die after 5 seconds!\n", stderr);

    s_gameHostMutex.lock();
    hostmap_t::iterator host_iter = s_gameHosts.begin();
    while (host_iter != s_gameHosts.end()) {
        if (host_iter->second == host)
            host_iter = s_gameHosts.erase(host_iter);
        else
            ++host_iter;
    }
    s_gameHostMutex.unlock();

    if (host->m_temp) {
        PostgresStrings<1> params;
        params.set(0, host->m_serverIdx);
        PQexecParams(host->m_postgres,
                "DELETE FROM game.\"Servers\" "
                "    WHERE \"idx\"=$1;",
                1, 0, params.m_values, 0, 0, 0);
    }
    PQfinish(host->m_postgres);
    delete host;
}

void dm_propagate(GameHost_Private* host, MOUL::NetMessage* msg, uint32_t sender)
{
    DM_WRITEMSG(host, msg);

    std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
    for (auto client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if (client_iter->second->m_clientInfo.m_PlayerId == sender
            && !(msg->m_contentFlags & MOUL::NetMessage::e_EchoBackToSender))
            continue;

        try {
            DS::CryptSendBuffer(client_iter->second->m_sock, client_iter->second->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
        } catch (DS::SockHup) {
            // This is handled below too, but we don't want to skip the rest
            // of the client list if one hung up
        }
    }
}

void dm_propagate_to(GameHost_Private* host, MOUL::NetMessage* msg,
                     const std::vector<uint32_t>& receivers)
{
    DM_WRITEMSG(host, msg);

    std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
    for (auto rcvr_iter = receivers.begin(); rcvr_iter != receivers.end(); ++rcvr_iter) {
        for (hostmap_t::iterator recv_host = s_gameHosts.begin(); recv_host != s_gameHosts.end(); ++recv_host) {
            auto client = recv_host->second->m_clients.find(*rcvr_iter);
            if (client != recv_host->second->m_clients.end()) {
                try {
                    DS::CryptSendBuffer(client->second->m_sock, client->second->m_crypt,
                                        host->m_buffer.buffer(), host->m_buffer.size());
                } catch (DS::SockHup) {
                    // This is handled below too, but we don't want to skip the rest
                    // of the client list if one hung up
                }
                break; // Don't bother checking the rest of the hosts, we found the one we're looking for
            }
        }
    }
}

void dm_game_disconnect(GameHost_Private* host, Game_ClientMessage* msg)
{
    // Unload the avatar clone if the client committed hara-kiri
    if (msg->m_client->m_isLoaded && !msg->m_client->m_clientKey.isNull())
    {
        MOUL::LoadCloneMsg* cloneMsg = MOUL::LoadCloneMsg::Create();
        cloneMsg->m_bcastFlags = MOUL::Message::e_NetPropagate
                               | MOUL::Message::e_LocalPropagate;
        cloneMsg->m_receivers.push_back(MOUL::Key::NetClientMgrKey);
        cloneMsg->m_cloneKey = msg->m_client->m_clientKey;
        cloneMsg->m_requestorKey = MOUL::Key::AvatarMgrKey;
        cloneMsg->m_userData = 0;
        cloneMsg->m_originPlayerId = msg->m_client->m_clientInfo.m_PlayerId;
        cloneMsg->m_validMsg = true;
        cloneMsg->m_isLoading = false;

        MOUL::NetMsgLoadClone* netMsg = MOUL::NetMsgLoadClone::Create();
        netMsg->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                               | MOUL::NetMessage::e_NeedsReliableSend;
        netMsg->m_timestamp.setNow();
        netMsg->m_isPlayer = true;
        netMsg->m_isLoading = false;
        netMsg->m_isInitialState = false;
        netMsg->m_message = cloneMsg;
        netMsg->m_object = msg->m_client->m_clientKey;

        dm_propagate(host, netMsg, msg->m_client->m_clientInfo.m_PlayerId);
        netMsg->unref();
    }

    MOUL::NetMsgMemberUpdate* memberMsg = MOUL::NetMsgMemberUpdate::Create();
    memberMsg->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                              | MOUL::NetMessage::e_IsSystemMessage
                              | MOUL::NetMessage::e_NeedsReliableSend;
    memberMsg->m_timestamp.setNow();
    memberMsg->m_member.m_client = msg->m_client->m_clientInfo;
    memberMsg->m_member.m_avatarKey = msg->m_client->m_clientKey;
    memberMsg->m_addMember = false;

    dm_propagate(host, memberMsg, msg->m_client->m_clientInfo.m_PlayerId);
    memberMsg->unref();
    SEND_REPLY(msg, DS::e_NetSuccess);

    // Release any stale locks
    host->m_lockMutex.lock();
    for (auto it = host->m_locks.begin(); it != host->m_locks.end(); )
    {
        if (it->second == msg->m_client->m_clientInfo.m_PlayerId)
            it = host->m_locks.erase(it);
        else
            ++it;
    }
    host->m_lockMutex.unlock();

    // Reassign game-mastership if needed...
    {
        std::lock_guard<std::mutex> gmGuard(host->m_gmMutex);
        if (host->m_gameMaster == msg->m_client->m_clientInfo.m_PlayerId) {
            MOUL::NetMsgGroupOwner* groupMsg = MOUL::NetMsgGroupOwner::Create();
            groupMsg->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                                    | MOUL::NetMessage::e_IsSystemMessage
                                    | MOUL::NetMessage::e_NeedsReliableSend;
            groupMsg->m_timestamp.setNow();
            groupMsg->m_groups.resize(1);
            groupMsg->m_groups[0].m_own = true;
            DM_WRITEMSG(host, groupMsg);

            // This client has already been removed from the map, so we can just
            // pick the 0th client and call him the new owner :)
            std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
            if (host->m_clients.size()) {
                GameClient_Private* newOwner = host->m_clients.begin()->second;
                host->m_gameMaster = newOwner->m_clientInfo.m_PlayerId;
                try {
                    DS::CryptSendBuffer(newOwner->m_sock, newOwner->m_crypt,
                                        host->m_buffer.buffer(), host->m_buffer.size());
                } catch (...) {
                    fprintf(stderr, "[Game] Ownership transfer to %i from %i failed.",
                            msg->m_client->m_clientInfo.m_PlayerId, newOwner->m_clientInfo.m_PlayerId);
                    DS_DASSERT(false);
                }
            } else
                host->m_gameMaster = 0;
            groupMsg->unref();
        }
    }

    // Good time to write this back to the vault
    Auth_NodeInfo sdlNode;
    AuthClient_Private fakeClient;
    sdlNode.m_client = &fakeClient;
    sdlNode.m_node.set_NodeIdx(host->m_sdlIdx);
    sdlNode.m_node.set_Blob_1(host->m_vaultState.toBlob());
    s_authChannel.putMessage(e_VaultUpdateNode, reinterpret_cast<void*>(&sdlNode));
    if (fakeClient.m_channel.getMessage().m_messageType != DS::e_NetSuccess)
        fputs("[Game] Error writing SDL node back to vault\n", stderr);

    // TODO: This should probably respect the age's LingerTime
    //       As it is, there might be a race condition if another player is
    //       joining just as the last player is leaving.
    if (host->m_clients.size() == 0)
        host->m_channel.putMessage(e_GameShutdown, 0);
}

void dm_game_join(GameHost_Private* host, Game_ClientMessage* msg)
{
    // This does a few things for us...
    //   1. We get proper age node subscriptions (vault downloaded after we reply to this req)
    //   2. We ensure that the player is logged in and is supposed to be coming here.
    AuthClient_Private fakeClient;
    Auth_UpdateAgeSrv authReq;
    authReq.m_client = &fakeClient;
    authReq.m_ageNodeId = host->m_ageIdx;
    authReq.m_playerId = msg->m_client->m_clientInfo.m_PlayerId;
    s_authChannel.putMessage(e_AuthUpdateAgeSrv, reinterpret_cast<void*>(&authReq));

    DS::FifoMessage authReply = fakeClient.m_channel.getMessage();
    if (authReply.m_messageType != DS::e_NetSuccess) {
        SEND_REPLY(msg, authReply.m_messageType);
        return;
    }

    // Simplified object ownership...
    // In MOUL, one player owns ALL synched objects
    // We'll call him the "game master"
    MOUL::NetMsgGroupOwner* groupMsg = MOUL::NetMsgGroupOwner::Create();
    groupMsg->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                             | MOUL::NetMessage::e_IsSystemMessage
                             | MOUL::NetMessage::e_NeedsReliableSend;
    groupMsg->m_timestamp.setNow();
    groupMsg->m_groups.resize(1);
    host->m_gmMutex.lock();
    if (!host->m_gameMaster) {
        groupMsg->m_groups[0].m_own = true;
        host->m_gameMaster = msg->m_client->m_clientInfo.m_PlayerId;
    } else
        groupMsg->m_groups[0].m_own = false;
    host->m_gmMutex.unlock();

    DM_WRITEMSG(host, groupMsg);
    try {
        DS::CryptSendBuffer(msg->m_client->m_sock, msg->m_client->m_crypt,
                            host->m_buffer.buffer(), host->m_buffer.size());
    } catch (DS::SockHup) {
        // we'll just let this slide. The client thread will figure it out quickly enough.
    }
    groupMsg->unref();

    MOUL::NetMsgMemberUpdate* memberMsg = MOUL::NetMsgMemberUpdate::Create();
    memberMsg->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                              | MOUL::NetMessage::e_IsSystemMessage
                              | MOUL::NetMessage::e_NeedsReliableSend;
    memberMsg->m_timestamp.setNow();
    memberMsg->m_member.m_client = msg->m_client->m_clientInfo;
    memberMsg->m_member.m_avatarKey = msg->m_client->m_clientKey;
    memberMsg->m_addMember = true;

    dm_propagate(host, memberMsg, msg->m_client->m_clientInfo.m_PlayerId);
    memberMsg->unref();

    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_send_state(GameHost_Private* host, GameClient_Private* client)
{
    check_postgres(host);

    MOUL::NetMsgSDLState* state = MOUL::NetMsgSDLState::Create();
    state->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                        | MOUL::NetMessage::e_NeedsReliableSend;
    state->m_timestamp.setNow();
    state->m_isInitial = true;
    state->m_persistOnServer = true;
    state->m_isAvatar = false;

    uint32_t states = 0;
    DS::Blob ageSdlBlob = host->m_vaultState.toBlob();
    if (ageSdlBlob.size()) {
        Game_AgeInfo info = s_ages[host->m_ageFilename];
        state->m_object.m_location = MOUL::Location::Make(info.m_seqPrefix, -2, MOUL::Location::e_BuiltIn);
        state->m_object.m_name = "AgeSDLHook";
        state->m_object.m_type = 1;  // SceneObject
        state->m_object.m_id = 1;
        state->m_sdlBlob = ageSdlBlob;
        DM_WRITEMSG(host, state);
        DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                            host->m_buffer.buffer(), host->m_buffer.size());
        ++states;
    }

    for (sdlstatemap_t::iterator state_iter = host->m_states.begin();
         state_iter != host->m_states.end(); ++state_iter) {
        for (sdlnamemap_t::iterator it = state_iter->second.begin();
             it != state_iter->second.end(); ++it) {
            state->m_object = state_iter->first;
            state->m_sdlBlob = it->second.toBlob();
            DM_WRITEMSG(host, state);
            DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
            ++states;
        }
    }
    state->unref();

    // Final message indicating the whole state was sent
    MOUL::NetMsgInitialAgeStateSent* reply = MOUL::NetMsgInitialAgeStateSent::Create();
    reply->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                          | MOUL::NetMessage::e_IsSystemMessage
                          | MOUL::NetMessage::e_NeedsReliableSend;
    reply->m_timestamp.setNow();
    reply->m_numStates = states;

    DM_WRITEMSG(host, reply);
    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
    reply->unref();
}

void dm_read_sdl(GameHost_Private* host, GameClient_Private* client,
                 MOUL::NetMsgSDLState* state, bool bcast)
{
    if (state->m_sdlBlob.size() == 0)
        return;

    SDL::State update;
    try {
        update = SDL::State::FromBlob(state->m_sdlBlob);
    } catch (DS::EofException) {
        fputs("[SDL] Error parsing state\n", stderr);
        return;
    }

#if 0  // Enable for SDL debugging
    fprintf(stderr, "[SDL] Bcasting SDL %s for [%04X]%s\n",
            update.descriptor()->m_name.c_str(), state->m_object.m_type,
            state->m_object.m_name.c_str());
    update.debug();
#endif
    if (state->m_object.m_name == "AgeSDLHook") {
        host->m_vaultState.add(update);
        Auth_NodeInfo sdlNode;
        AuthClient_Private fakeClient;
        sdlNode.m_client = &fakeClient;
        sdlNode.m_node.set_NodeIdx(host->m_sdlIdx);
        sdlNode.m_node.set_Blob_1(host->m_vaultState.toBlob());
        sdlNode.m_revision = DS::Uuid();
        s_authChannel.putMessage(e_VaultUpdateNode, reinterpret_cast<void*>(&sdlNode));
        if (fakeClient.m_channel.getMessage().m_messageType != DS::e_NetSuccess)
            fputs("[Game] Error writing SDL node back to vault\n", stderr);
    } else if (state->m_isAvatar) {
        if (client->m_states.find(update.descriptor()->m_name) == client->m_states.end())
            client->m_states[update.descriptor()->m_name] = update;
        else
            client->m_states[update.descriptor()->m_name].add(update);
    } else {
        sdlstatemap_t::iterator fobj = host->m_states.find(state->m_object);
        if (fobj == host->m_states.end() || fobj->second.find(update.descriptor()->m_name) == fobj->second.end())
            host->m_states[state->m_object][update.descriptor()->m_name] = update;
        else
            host->m_states[state->m_object][update.descriptor()->m_name].add(update);

        if (state->m_persistOnServer) {
            check_postgres(host);

            PostgresStrings<4> parms;
            host->m_buffer.truncate();
            state->m_object.write(&host->m_buffer);
            parms.set(0, host->m_serverIdx);
            parms.set(1, update.descriptor()->m_name);
            parms.set(2, DS::Base64Encode(host->m_buffer.buffer(), host->m_buffer.size()));
            parms.set(3, DS::Base64Encode(state->m_sdlBlob.buffer(), state->m_sdlBlob.size()));
            PGresult* result = PQexecParams(host->m_postgres,
                "SELECT idx FROM game.\"AgeStates\""
                "    WHERE \"ServerIdx\"=$1 AND \"Descriptor\"=$2 AND \"ObjectKey\"=$3",
                3, 0, parms.m_values, 0, 0, 0);
            if (PQresultStatus(result) != PGRES_TUPLES_OK) {
                fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                        __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
                PQclear(result);
                return;
            }
            if (PQntuples(result) == 0) {
                PQclear(result);
                result = PQexecParams(host->m_postgres,
                    "INSERT INTO game.\"AgeStates\""
                    "    (\"ServerIdx\", \"Descriptor\", \"ObjectKey\", \"SdlBlob\")"
                    "    VALUES ($1, $2, $3, $4)",
                    4, 0, parms.m_values, 0, 0, 0);
                if (PQresultStatus(result) != PGRES_COMMAND_OK) {
                    fprintf(stderr, "%s:%d:\n    Postgres INSERT error: %s\n",
                            __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
                    PQclear(result);
                    return;
                }
                PQclear(result);
            } else {
                DS_DASSERT(PQntuples(result) == 1);
                parms.set(0, DS::String(PQgetvalue(result, 0, 0)));
                parms.set(1, parms.m_strings[3]);   // SDL blob
                PQclear(result);
                result = PQexecParams(host->m_postgres,
                    "UPDATE game.\"AgeStates\""
                    "    SET \"SdlBlob\"=$2 WHERE idx=$1",
                    2, 0, parms.m_values, 0, 0, 0);
                if (PQresultStatus(result) != PGRES_COMMAND_OK) {
                    fprintf(stderr, "%s:%d:\n    Postgres UPDATE error: %s\n",
                            __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
                    PQclear(result);
                    return;
                }
                PQclear(result);
            }
        }
    }

    if (bcast)
        dm_propagate(host, state, client->m_clientInfo.m_PlayerId);
}

void dm_test_and_set(GameHost_Private* host, GameClient_Private* client,
                     MOUL::NetMsgTestAndSet* msg)
{
    MOUL::ServerReplyMsg* reply = MOUL::ServerReplyMsg::Create();
    reply->m_receivers.push_back(msg->m_object);
    reply->m_bcastFlags = MOUL::Message::e_LocalPropagate;

    {
        std::lock_guard<std::mutex> lockGuard(host->m_lockMutex);
        if (msg->m_lockRequest) {
            if (host->m_locks.find(msg->m_object) == host->m_locks.end()) {
                reply->m_reply = MOUL::ServerReplyMsg::e_Affirm;
                host->m_locks[msg->m_object] = client->m_clientInfo.m_PlayerId;
            } else {
                reply->m_reply = MOUL::ServerReplyMsg::e_Deny;
            }
        } else {
            auto it = host->m_locks.find(msg->m_object);
            if (it != host->m_locks.end() && it->second == client->m_clientInfo.m_PlayerId) {
                host->m_locks.erase(it);
            }
            return;
        }
    }

    MOUL::NetMsgGameMessage* netReply = MOUL::NetMsgGameMessage::Create();
    netReply->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                             | MOUL::NetMessage::e_NeedsReliableSend;
    netReply->m_timestamp.setNow();
    netReply->m_message = reply;

    DM_WRITEMSG(host, netReply);
    netReply->unref();
    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
}

void dm_send_members(GameHost_Private* host, GameClient_Private* client)
{
    MOUL::NetMsgMembersList* members = MOUL::NetMsgMembersList::Create();
    members->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                            | MOUL::NetMessage::e_HasPlayerID
                            | MOUL::NetMessage::e_IsSystemMessage
                            | MOUL::NetMessage::e_NeedsReliableSend;
    members->m_timestamp.setNow();
    members->m_playerId = client->m_clientInfo.m_PlayerId;

    host->m_clientMutex.lock();
    members->m_members.reserve(host->m_clients.size() - 1);
    for (auto client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if (client_iter->second->m_clientInfo.m_PlayerId != client->m_clientInfo.m_PlayerId
            && !client->m_clientKey.isNull()) {
            MOUL::NetMsgMemberInfo info;
            info.m_client = client_iter->second->m_clientInfo;
            info.m_avatarKey = client_iter->second->m_clientKey;
            members->m_members.push_back(info);
        }
    }
    host->m_clientMutex.unlock();

    DM_WRITEMSG(host, members);
    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                        host->m_buffer.buffer(), host->m_buffer.size());
    members->unref();

    // Load non-avatar clones (ie NPC quabs)
    {
        std::lock_guard<std::mutex> cloneGuard(host->m_cloneMutex);
        for (auto clone_iter = host->m_clones.begin(); clone_iter != host->m_clones.end(); ++clone_iter)
        {
            DM_WRITEMSG(host, clone_iter->second);
            DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
        }
    }

    // Load clones for players already in the age
    MOUL::NetMsgLoadClone* cloneMsg = MOUL::NetMsgLoadClone::Create();
    cloneMsg->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                             | MOUL::NetMessage::e_NeedsReliableSend;
    cloneMsg->m_timestamp.setNow();
    cloneMsg->m_isPlayer = true;
    cloneMsg->m_isLoading = true;
    cloneMsg->m_isInitialState = true;

    MOUL::LoadAvatarMsg* avatarMsg = MOUL::LoadAvatarMsg::Create();
    avatarMsg->m_bcastFlags = MOUL::Message::e_NetPropagate
                            | MOUL::Message::e_LocalPropagate;
    avatarMsg->m_receivers.push_back(MOUL::Key::NetClientMgrKey);
    avatarMsg->m_requestorKey = MOUL::Key::AvatarMgrKey;
    avatarMsg->m_userData = 0;
    avatarMsg->m_validMsg = true;
    avatarMsg->m_isLoading = true;
    avatarMsg->m_isPlayer = true;
    cloneMsg->m_message = avatarMsg;

    {
        std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
        for (auto client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
            if (client_iter->second->m_clientInfo.m_PlayerId != client->m_clientInfo.m_PlayerId
                && !client->m_clientKey.isNull()) {
                avatarMsg->m_cloneKey = client_iter->second->m_clientKey;
                avatarMsg->m_originPlayerId = client_iter->second->m_clientInfo.m_PlayerId;

                DM_WRITEMSG(host, cloneMsg);
                DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                                    host->m_buffer.buffer(), host->m_buffer.size());

                MOUL::NetMsgSDLState* state = MOUL::NetMsgSDLState::Create();
                state->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                                    | MOUL::NetMessage::e_NeedsReliableSend;
                state->m_timestamp.setNow();
                // Blame Cyan for these odd flags
                state->m_isInitial = true;
                state->m_persistOnServer = true;
                state->m_isAvatar = false;
                state->m_object = client_iter->second->m_clientKey;
                for (auto sdl_iter = client_iter->second->m_states.begin(); sdl_iter != client_iter->second->m_states.end(); ++sdl_iter) {
                    state->m_sdlBlob = sdl_iter->second.toBlob();
                    DM_WRITEMSG(host, state);
                    DS::CryptSendBuffer(client->m_sock, client->m_crypt,
                                        host->m_buffer.buffer(), host->m_buffer.size());
                }
                state->unref();
            }
        }
    }
    cloneMsg->unref();
}

void dm_load_clone(GameHost_Private* host, GameClient_Private* client,
                   MOUL::NetMsgLoadClone* netmsg)
{
    MOUL::LoadCloneMsg* msg = netmsg->m_message->Cast<MOUL::LoadCloneMsg>();
    if (msg->makeSafeForNet())
    {
        if (netmsg->m_isPlayer)
        {
            host->m_clientMutex.lock();
            client->m_clientKey = netmsg->m_object;
            client->m_isLoaded = netmsg->m_isLoading;
            host->m_clientMutex.unlock();
        }
        else
        {
            std::lock_guard<std::mutex> cloneGuard(host->m_cloneMutex);
            auto it = host->m_clones.find(netmsg->m_object);
            if (it != host->m_clones.end())
            {
                it->second->unref();
                // If, for some reason, the client decides to send a dupe (it can happen...)
                if (netmsg->m_isLoading)
                    host->m_clones[netmsg->m_object] = netmsg;
                else
                    host->m_clones.erase(it);
            }
            else if (netmsg->m_isLoading)
            {
                netmsg->ref();
                host->m_clones[netmsg->m_object] = netmsg;
            }
        }
        dm_propagate(host, netmsg, client->m_clientInfo.m_PlayerId);
    }
}

void dm_game_message(GameHost_Private* host, Game_PropagateMessage* msg)
{
    DS::BlobStream stream(msg->m_message);
    MOUL::NetMessage* netmsg = 0;
    try {
        netmsg = MOUL::Factory::Read<MOUL::NetMessage>(&stream);
    } catch (const MOUL::FactoryException&) {
        fprintf(stderr, "[Game] Warning: Ignoring message: %04X\n",
                msg->m_messageType);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    } catch (const DS::AssertException& ex) {
        fprintf(stderr, "[Game] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    } catch (const std::exception& ex) {
        // magickal code to print out the name of the offending plMessage
        MOUL::NetMsgGameMessage* gameMsg = netmsg->Cast<MOUL::NetMsgGameMessage>();
        if (gameMsg && gameMsg->m_message) {
            switch (gameMsg->m_message->type()) {
#define CREATABLE_TYPE(id, name) \
            case id: \
                fprintf(stderr, "[Game] Unknown exception reading " #name ": %s\n", ex.what()); \
                break;
#include "creatable_types.inl"
#undef CREATABLE_TYPE
            }
        } else {
            fprintf(stderr, "[Game] Unknown exception reading net message: %s\n",
                    ex.what());
        }
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
#ifdef DEBUG
    if (!stream.atEof()) {
        fprintf(stderr, "[Game] Incomplete parse of %04X\n", netmsg->type());
        netmsg->unref();
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
#endif

    try {
        switch (msg->m_messageType) {
        case MOUL::ID_NetMsgPagingRoom:
            dm_propagate(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
            break;
        case MOUL::ID_NetMsgGameStateRequest:
            dm_send_state(host, msg->m_client);
            break;
        case MOUL::ID_NetMsgGameMessage:
            {
                MOUL::NetMsgGameMessage* gameMsg = netmsg->Cast<MOUL::NetMsgGameMessage>();
                gameMsg->m_message->m_bcastFlags |= MOUL::Message::e_NetNonLocal;
                if (gameMsg->m_message->makeSafeForNet())
                    dm_propagate(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
            }
            break;
        case MOUL::ID_NetMsgGameMessageDirected:
            {
                MOUL::NetMsgGameMessageDirected* directedMsg =
                        netmsg->Cast<MOUL::NetMsgGameMessageDirected>();
                directedMsg->m_message->m_bcastFlags |= MOUL::Message::e_NetNonLocal;
                if (directedMsg->m_message->makeSafeForNet())
                    dm_propagate_to(host, netmsg, directedMsg->m_receivers);
            }
            break;
        case MOUL::ID_NetMsgVoice:
            dm_propagate_to(host, netmsg, netmsg->Cast<MOUL::NetMsgVoice>()->m_receivers);
            break;
        case MOUL::ID_NetMsgTestAndSet:
            dm_test_and_set(host, msg->m_client, netmsg->Cast<MOUL::NetMsgTestAndSet>());
            break;
        case MOUL::ID_NetMsgMembersListReq:
            dm_send_members(host, msg->m_client);
            break;
        case MOUL::ID_NetMsgSDLState:
            dm_read_sdl(host, msg->m_client, netmsg->Cast<MOUL::NetMsgSDLState>(), false);
            break;
        case MOUL::ID_NetMsgSDLStateBCast:
            dm_read_sdl(host, msg->m_client, netmsg->Cast<MOUL::NetMsgSDLState>(), true);
            break;
        case MOUL::ID_NetMsgRelevanceRegions:
            //TODO: Filter messages by client's requested relevance regions
            break;
        case MOUL::ID_NetMsgLoadClone:
            dm_load_clone(host, msg->m_client, netmsg->Cast<MOUL::NetMsgLoadClone>());
            break;
        case MOUL::ID_NetMsgPlayerPage:
            //TODO: Whatever the client is expecting us to do
            break;
        default:
            fprintf(stderr, "[Game] Warning: Unhandled message: %04X\n",
                    msg->m_messageType);
            break;
        }
    } catch (DS::SockHup) {
        // Client wasn't paying attention
    }
    netmsg->unref();
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_sdl_update(GameHost_Private* host, Game_SdlMessage* msg) {
    SDL::State vaultState = SDL::State::FromBlob(msg->m_node.m_Blob_1);
    host->m_vaultState.merge(vaultState);
    msg->m_node.m_Blob_1 = host->m_vaultState.toBlob();

    Auth_NodeInfo sdlNode;
    AuthClient_Private fakeClient;
    sdlNode.m_client = &fakeClient;
    sdlNode.m_node = msg->m_node;
    sdlNode.m_revision = DS::Uuid();
    s_authChannel.putMessage(e_VaultUpdateNode, reinterpret_cast<void*>(&sdlNode));
    if (fakeClient.m_channel.getMessage().m_messageType != DS::e_NetSuccess)
        fputs("[Game] Error writing SDL node back to vault\n", stderr);

    Game_AgeInfo info = s_ages[host->m_ageFilename];

    MOUL::NetMsgSDLStateBCast* bcast = MOUL::NetMsgSDLStateBCast::Create(); //MOUL::Factory::Create(MOUL::ID_NetMsgSDLStateBCast);
    bcast->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                            | MOUL::NetMessage::e_NeedsReliableSend;
    bcast->m_timestamp.setNow();
    bcast->m_isInitial = true;
    bcast->m_persistOnServer = true;
    bcast->m_isAvatar = false;
    bcast->m_object.m_location = MOUL::Location::Make(info.m_seqPrefix, -2, MOUL::Location::e_BuiltIn);
    bcast->m_object.m_name = "AgeSDLHook";
    bcast->m_object.m_type = 1;  // SceneObject
    bcast->m_object.m_id = 1;
    bcast->m_sdlBlob = msg->m_node.m_Blob_1;

    std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
    for (auto client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        try {
            DM_WRITEMSG(host, bcast);
            DS::CryptSendBuffer(client_iter->second->m_sock, client_iter->second->m_crypt,
                                host->m_buffer.buffer(), host->m_buffer.size());
        } catch (DS::SockHup) {
            // This is handled below too, but we don't want to skip the rest
            // of the client list if one hung up
        }
    }

    delete bcast;
    delete msg;
}

void dm_gameHost(GameHost_Private* host)
{
    for ( ;; ) {
        DS::FifoMessage msg = host->m_channel.getMessage();
        try {
            switch (msg.m_messageType) {
            case e_GameShutdown:
                dm_game_shutdown(host);
                return;
            case e_GameDisconnect:
                dm_game_disconnect(host, reinterpret_cast<Game_ClientMessage*>(msg.m_payload));
                break;
            case e_GameJoinAge:
                dm_game_join(host, reinterpret_cast<Game_ClientMessage*>(msg.m_payload));
                break;
            case e_GamePropagate:
                dm_game_message(host, reinterpret_cast<Game_PropagateMessage*>(msg.m_payload));
                break;
            case e_GameSdlUpdate:
                dm_sdl_update(host, reinterpret_cast<Game_SdlMessage*>(msg.m_payload));
                break;
            default:
                /* Invalid message...  This shouldn't happen */
                DS_DASSERT(0);
                break;
            }
        } catch (DS::AssertException ex) {
            fprintf(stderr, "[Game] Assertion failed at %s:%ld:  %s\n",
                    ex.m_file, ex.m_line, ex.m_cond);
            if (msg.m_payload) {
                // Keep clients from blocking on a reply
                SEND_REPLY(reinterpret_cast<Game_ClientMessage*>(msg.m_payload),
                           DS::e_NetInternalError);
            }
        }
    }

    dm_game_shutdown(host);
}

GameHost_Private* start_game_host(uint32_t ageMcpId)
{
    PGconn* postgres = PQconnectdb(DS::String::Format(
                    "host='%s' port='%s' user='%s' password='%s' dbname='%s'",
                    DS::Settings::DbHostname(), DS::Settings::DbPort(),
                    DS::Settings::DbUsername(), DS::Settings::DbPassword(),
                    DS::Settings::DbDbaseName()).c_str());
    if (PQstatus(postgres) != CONNECTION_OK) {
        fprintf(stderr, "Error connecting to postgres: %s", PQerrorMessage(postgres));
        PQfinish(postgres);
        return 0;
    }

    PostgresStrings<1> parms;
    parms.set(0, ageMcpId);
    PGresult* result = PQexecParams(postgres,
            "SELECT \"AgeUuid\", \"AgeFilename\", \"AgeIdx\", \"SdlIdx\", \"Temporary\""
            "    FROM game.\"Servers\" WHERE idx=$1",
            1, 0, parms.m_values, 0, 0, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                __FILE__, __LINE__, PQerrorMessage(postgres));
        PQclear(result);
        PQfinish(postgres);
        return 0;
    }
    if (PQntuples(result) == 0) {
        fprintf(stderr, "[Game] Age MCP %u not found\n", ageMcpId);
        PQclear(result);
        PQfinish(postgres);
        return 0;
    } else {
        DS_DASSERT(PQntuples(result) == 1);

        GameHost_Private* host = new GameHost_Private();
        host->m_instanceId = PQgetvalue(result, 0, 0);
        host->m_ageFilename = PQgetvalue(result, 0, 1);
        host->m_ageIdx = strtoul(PQgetvalue(result, 0, 2), 0, 10);
        host->m_gameMaster = 0;
        host->m_serverIdx = ageMcpId;
        host->m_postgres = postgres;
        host->m_temp = strcmp("t", PQgetvalue(result, 0, 4)) == 0;

        // Fetch the vault's SDL blob
        unsigned long sdlidx = strtoul(PQgetvalue(result, 0, 3), 0, 10);
        if (sdlidx) {
            Auth_NodeInfo sdlFind;
            AuthClient_Private fakeClient;
            sdlFind.m_client = &fakeClient;
            sdlFind.m_node.set_NodeIdx(sdlidx);
            s_authChannel.putMessage(e_VaultFetchNode, reinterpret_cast<void*>(&sdlFind));
            DS::FifoMessage reply = fakeClient.m_channel.getMessage();
            if (reply.m_messageType != DS::e_NetSuccess) {
                fputs("[Game] Error fetching Age SDL\n", stderr);
                PQclear(result);
                PQfinish(postgres);
                delete host;
                return 0;
            }
            host->m_sdlIdx = sdlFind.m_node.m_NodeIdx;
            try {
                host->m_vaultState = SDL::State::FromBlob(sdlFind.m_node.m_Blob_1);
                host->m_vaultState.update();
            } catch (DS::EofException) {
                fprintf(stderr, "[SDL] Error parsing Age SDL state for %s\n",
                        host->m_ageFilename.c_str());
            }
        } else {
            host->m_vaultState = SDL::State::FromBlob(gen_default_sdl(host->m_ageFilename));
        }

        s_gameHostMutex.lock();
        s_gameHosts[ageMcpId] = host;
        s_gameHostMutex.unlock();
        PQclear(result);

        // Fetch initial server state
        PostgresStrings<1> parms;
        parms.set(0, host->m_serverIdx);
        PGresult* result = PQexecParams(host->m_postgres,
                "SELECT \"Descriptor\", \"ObjectKey\", \"SdlBlob\""
                "    FROM game.\"AgeStates\" WHERE \"ServerIdx\"=$1",
                1, 0, parms.m_values, 0, 0, 0);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            fprintf(stderr, "%s:%d:\n    Postgres SELECT error: %s\n",
                    __FILE__, __LINE__, PQerrorMessage(host->m_postgres));
        } else {
            int count = PQntuples(result);

            for (int i=0; i<count; ++i) {
                DS::BlobStream bsObject(DS::Base64Decode(PQgetvalue(result, i, 1)));
                DS::Blob sdlblob = DS::Base64Decode(PQgetvalue(result, i, 2));
                MOUL::Uoid key;
                key.read(&bsObject);
                try {
                    SDL::State state = SDL::State::FromBlob(sdlblob);
                    state.update();
                    host->m_states[key][PQgetvalue(result, i, 0)] = state;
                } catch (DS::EofException) {
                    fprintf(stderr, "[SDL] Error parsing state %s for [%04X]%s\n",
                            PQgetvalue(result, i, 0), key.m_type, key.m_name.c_str());
                }
            }
        }
        PQclear(result);

        std::thread threadh(&dm_gameHost, host);
        threadh.detach();
        return host;
    }
}
