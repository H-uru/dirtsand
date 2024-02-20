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
#include <string_theory/codecs>
#include <string_theory/format>

hostmap_t s_gameHosts;
std::mutex s_gameHostMutex;
agemap_t s_ages;

#define SEND_REPLY(msg, result) \
    msg->m_client->m_channel.putMessage(result)

#define DM_SENDBUF(client) \
    _msgbuf->ref(); \
    client->m_broadcast.putMessage(e_GameToCli_PropagateBuffer, _msgbuf)

#define DM_UNREFBUF() \
    _msgbuf->unref()

#define DM_WRITEBUF(msg) \
    DS::BufferStream* _msgbuf = new DS::BufferStream(); \
    _msgbuf->write<uint32_t>(msg->type()); \
    _msgbuf->write<uint32_t>(0); \
    MOUL::Factory::WriteCreatable(_msgbuf, msg); \
    _msgbuf->seek(4, SEEK_SET); \
    _msgbuf->write<uint32_t>(_msgbuf->size() - 8)

#define DM_SENDMSG(msg, client) \
    DM_WRITEBUF(msg); \
    client->m_broadcast.putMessage(e_GameToCli_PropagateBuffer, _msgbuf)

void dm_game_shutdown(GameHost_Private* host)
{
    {
        std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
        for (auto client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter)
            DS::CloseSock(client_iter->second->m_sock);
    }

    for (auto clone_iter = host->m_clones.begin(); clone_iter != host->m_clones.end(); ++clone_iter)
        clone_iter->second->unref();
    host->m_clones.clear();

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
        DS::PQexecVA(host->m_postgres,
                     "DELETE FROM game.\"Servers\" "
                     "    WHERE \"idx\"=$1",
                     host->m_serverIdx);
    }
    PQfinish(host->m_postgres);
    delete host;
}

void dm_broadcast(GameHost_Private* host, MOUL::NetMessage* msg, uint32_t sender)
{
    DM_WRITEBUF(msg);

    std::lock_guard<std::mutex> hostGuard(s_gameHostMutex);
    for (auto host_it = s_gameHosts.begin(); host_it != s_gameHosts.end(); ++host_it) {
        std::lock_guard<std::mutex> clientGuard(host_it->second->m_clientMutex);
        for (auto client_it = host_it->second->m_clients.begin(); client_it != host_it->second->m_clients.end(); ++client_it) {
            if (client_it->second->m_clientInfo.m_PlayerId == sender
                && !(msg->m_contentFlags & MOUL::NetMessage::e_EchoBackToSender))
                continue;
            DM_SENDBUF(client_it->second);
        }
    }

    DM_UNREFBUF();
}

void dm_propagate(GameHost_Private* host, MOUL::NetMessage* msg, uint32_t sender)
{
    DM_WRITEBUF(msg);

    std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
    for (auto client_iter = host->m_clients.begin(); client_iter != host->m_clients.end(); ++client_iter) {
        if (client_iter->second->m_clientInfo.m_PlayerId == sender
            && !(msg->m_contentFlags & MOUL::NetMessage::e_EchoBackToSender))
            continue;
        DM_SENDBUF(client_iter->second);
    }

    DM_UNREFBUF();
}

void dm_propagate_to(GameHost_Private* host, MOUL::NetMessage* msg,
                     const std::vector<uint32_t>& receivers)
{
    DM_WRITEBUF(msg);

    for (auto rcvr_iter = receivers.begin(); rcvr_iter != receivers.end(); ++rcvr_iter) {
        std::lock_guard<std::mutex> hostGuard(s_gameHostMutex);
        for (hostmap_t::iterator recv_host = s_gameHosts.begin(); recv_host != s_gameHosts.end(); ++recv_host) {
            std::lock_guard<std::mutex> clientGuard(recv_host->second->m_clientMutex);
            auto client = recv_host->second->m_clients.find(*rcvr_iter);
            if (client != recv_host->second->m_clients.end()) {
                DM_SENDBUF(client->second);
                break; // Don't bother checking the rest of the hosts, we found the one we're looking for
            }
        }
    }

    DM_UNREFBUF();
}

void dm_local_sdl_update(GameHost_Private* host, DS::Blob blob)
{
    Auth_NodeInfo sdlNode;
    AuthClient_Private fakeClient;
    sdlNode.m_client = &fakeClient;
    sdlNode.m_internal = true;
    sdlNode.m_node.set_NodeIdx(host->m_sdlIdx);
    sdlNode.m_node.set_Blob_1(std::move(blob));
    s_authChannel.putMessage(e_VaultUpdateNode, reinterpret_cast<void*>(&sdlNode));
    if (fakeClient.m_channel.getMessage().m_messageType != DS::e_NetSuccess)
        fputs("[Game] Error writing SDL node back to vault\n", stderr);
}

void dm_game_disconnect(GameHost_Private* host, Game_ClientMessage* msg)
{
    // Unload the avatar clone if the client committed hara-kiri
    if (msg->m_client->m_isLoaded && !msg->m_client->m_clientKey.isNull()) {
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

        host->m_states.erase(msg->m_client->m_clientKey);
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

    // Release any stale locks
    host->m_lockMutex.lock();
    for (auto it = host->m_locks.begin(); it != host->m_locks.end(); ) {
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
            DM_WRITEBUF(groupMsg);

            // This client has already been removed from the map, so we can just
            // pick the 0th client and call him the new owner :)
            std::lock_guard<std::mutex> clientGuard(host->m_clientMutex);
            if (host->m_clients.size()) {
                GameClient_Private* newOwner = host->m_clients.begin()->second;
                host->m_gameMaster = newOwner->m_clientInfo.m_PlayerId;
                DM_SENDBUF(newOwner);
            } else {
                host->m_gameMaster = 0;
            }
            groupMsg->unref();
            DM_UNREFBUF();
        }
    }

    // Good time to write this back to the vault
    dm_local_sdl_update(host, host->m_localState.toBlob());

    // TODO: This should probably respect the age's LingerTime
    //       As it is, there might be a race condition if another player is
    //       joining just as the last player is leaving.
    if (host->m_clients.size() == 0)
        host->m_channel.putMessage(e_GameShutdown, nullptr);

    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_game_join(GameHost_Private* host, Game_ClientMessage* msg)
{
    // This does a few things for us...
    //   1. We get proper age node subscriptions (vault downloaded after we reply to this req)
    //   2. We ensure that the player is logged in and is supposed to be coming here.
    //   3. We learn if the client is an admin (can send naughty messages)
    AuthClient_Private fakeClient;
    Auth_UpdateAgeSrv authReq;
    authReq.m_client = &fakeClient;
    authReq.m_ageNodeId = host->m_ageIdx;
    authReq.m_playerId = msg->m_client->m_clientInfo.m_PlayerId;
    s_authChannel.putMessage(e_AuthUpdateAgeSrv, reinterpret_cast<void*>(&authReq));

    DS::FifoMessage authReply = fakeClient.m_channel.getMessage();
    msg->m_client->m_isAdmin = authReq.m_isAdmin;
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
    } else {
        groupMsg->m_groups[0].m_own = false;
    }
    host->m_gmMutex.unlock();

    DM_SENDMSG(groupMsg, msg->m_client);
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
    check_postgres(host->m_postgres);

    MOUL::NetMsgSDLState* state = MOUL::NetMsgSDLState::Create();
    state->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                        | MOUL::NetMessage::e_NeedsReliableSend;
    state->m_timestamp.setNow();
    state->m_isInitial = true;
    state->m_persistOnServer = true;
    state->m_isAvatar = false;

    uint32_t states = 0;
    DS::Blob ageSdlBlob = host->m_ageSdlHook.toBlob();
    if (ageSdlBlob.size()) {
        Game_AgeInfo info = s_ages[host->m_ageFilename];
        state->m_object.m_location = MOUL::Location(info.m_seqPrefix, -2, MOUL::Location::e_BuiltIn);
        state->m_object.m_name = "AgeSDLHook";
        state->m_object.m_type = 1;  // SceneObject
        state->m_object.m_id = 1;
        state->m_sdlBlob = std::move(ageSdlBlob);
        DM_SENDMSG(state, client);
        ++states;
    }

    for (sdlstatemap_t::iterator state_iter = host->m_states.begin();
         state_iter != host->m_states.end(); ++state_iter) {
        for (sdlnamemap_t::iterator it = state_iter->second.begin();
             it != state_iter->second.end(); ++it) {
            state->m_object = state_iter->first;
            state->m_isAvatar = it->second.m_isAvatar;
            state->m_persistOnServer = it->second.m_persist;
            state->m_sdlBlob = it->second.m_state.toBlob();
            DM_SENDMSG(state, client);
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
    DM_SENDMSG(reply, client);
    reply->unref();
}

void dm_save_sdl_state(GameHost_Private* host, const ST::string& descriptor,
                       const MOUL::Uoid& object, const SDL::State& state)
{
    check_postgres(host->m_postgres);

    DS::Blob sdlBlob = state.toBlob();
    DS::BufferStream buffer;
    object.write(&buffer);
    const ST::string object_b64 = ST::base64_encode(buffer.buffer(), buffer.size());
    const ST::string blob_b64 = ST::base64_encode(sdlBlob.buffer(), sdlBlob.size());
    DS::PGresultRef result = DS::PQexecVA(host->m_postgres,
            "SELECT idx FROM game.\"AgeStates\""
            "    WHERE \"ServerIdx\"=$1 AND \"Descriptor\"=$2 AND \"ObjectKey\"=$3",
            host->m_serverIdx, descriptor, object_b64);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQ_PRINT_ERROR(host->m_postgres, SELECT);
        return;
    }
    if (PQntuples(result) == 0) {
        result = DS::PQexecVA(host->m_postgres,
                              "INSERT INTO game.\"AgeStates\""
                              "    (\"ServerIdx\", \"Descriptor\", \"ObjectKey\", \"SdlBlob\")"
                              "    VALUES ($1, $2, $3, $4)",
                              host->m_serverIdx, descriptor, object_b64, blob_b64);
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQ_PRINT_ERROR(host->m_postgres, INSERT);
            return;
        }
    } else {
        if (PQntuples(result) != 1)
            fputs("Warning: Multiple rows returned for age state\n", stderr);
        const ST::string stateIdx(PQgetvalue(result, 0, 0));
        result = DS::PQexecVA(host->m_postgres,
                              "UPDATE game.\"AgeStates\""
                              "    SET \"SdlBlob\"=$2 WHERE idx=$1",
                              stateIdx, blob_b64);
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQ_PRINT_ERROR(host->m_postgres, UPDATE);
            return;
        }
    }
}

void dm_bcast_sdl_state(GameHost_Private* host, GameClient_Private* client,
                        const MOUL::NetMsgSDLState* update, const SDL::State& state)
{
    MOUL::NetMsgSDLStateBCast* bcast = MOUL::NetMsgSDLStateBCast::Create();
    bcast->m_contentFlags = MOUL::NetMessage::e_HasPlayerID
                          | MOUL::NetMessage::e_HasTimeSent
                          | MOUL::NetMessage::e_IsSystemMessage
                          | MOUL::NetMessage::e_NeedsReliableSend;
    bcast->m_timestamp.setNow();
    bcast->m_isAvatar = update->m_isAvatar;
    bcast->m_isInitial = update->m_isInitial;
    bcast->m_object = update->m_object;
    bcast->m_persistOnServer = update->m_persistOnServer;
    bcast->m_playerId = client->m_clientInfo.m_PlayerId;
    bcast->m_sdlBlob = state.toBlob();
    bcast->m_timestamp.setNow();
    dm_propagate(host, bcast, bcast->m_playerId);
    bcast->unref();
}

void dm_read_sdl(GameHost_Private* host, GameClient_Private* client,
                 MOUL::NetMsgSDLState* state, bool bcast)
{
    if (state->m_sdlBlob.size() == 0)
        return;

    SDL::State update;
    try {
        update = SDL::State::FromBlob(state->m_sdlBlob);
    } catch (const std::exception& ex) {
        ST::printf(stderr, "[SDL] Error parsing state: {}\n", ex.what());
        return;
    }

    if (!update.descriptor()) {
        ST::printf(stderr, "[SDL] Received an update for '{}' using an invalid descriptor!\n",
                   state->m_object.m_name);
        return;
    }

#if 0  // Enable for SDL debugging
    ST::printf(stderr, "[SDL] Bcasting SDL {} for [{04X}]{}\n",
               update.descriptor()->m_name, state->m_object.m_type,
               state->m_object.m_name);
    update.debug();
#endif

    if (state->m_object.m_name == "AgeSDLHook") {
        host->m_localState.add(update);
        host->m_ageSdlHook.add(update);
        dm_local_sdl_update(host, host->m_localState.toBlob());
        if (bcast)
            dm_bcast_sdl_state(host, client, state, host->m_ageSdlHook);
    } else {
        auto fobj = host->m_states.find(state->m_object);
        if (fobj == host->m_states.end() || fobj->second.find(update.descriptor()->m_name) == fobj->second.end()) {
            GameState gs;
            gs.m_isAvatar = state->m_isAvatar;
            gs.m_persist = state->m_persistOnServer;
            gs.m_state = update;
            host->m_states[state->m_object][update.descriptor()->m_name] = gs;

            if (state->m_persistOnServer)
                dm_save_sdl_state(host, update.descriptor()->m_name, state->m_object, update);
            if (bcast)
                dm_bcast_sdl_state(host, client, state, update);
        } else {
            GameState& gs = host->m_states[state->m_object][update.descriptor()->m_name];
            gs.m_isAvatar = state->m_isAvatar;
            gs.m_persist = state->m_persistOnServer;
            gs.m_state.add(update);

            if (state->m_persistOnServer)
                dm_save_sdl_state(host, update.descriptor()->m_name, state->m_object, gs.m_state);
            if (bcast)
                dm_bcast_sdl_state(host, client, state, gs.m_state);
        }
    }
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
            if (it != host->m_locks.end() && it->second == client->m_clientInfo.m_PlayerId)
                host->m_locks.erase(it);
            reply->unref();
            return;
        }
    }

    MOUL::NetMsgGameMessage* netReply = MOUL::NetMsgGameMessage::Create();
    netReply->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                             | MOUL::NetMessage::e_NeedsReliableSend;
    netReply->m_timestamp.setNow();
    netReply->m_message = reply;
    DM_SENDMSG(netReply, client);
    netReply->unref();
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

    DM_SENDMSG(members, client);
    members->unref();

    // Load non-avatar clones (ie NPC quabs)
    for (auto clone_iter = host->m_clones.begin(); clone_iter != host->m_clones.end(); ++clone_iter) {
        DM_SENDMSG(clone_iter->second, client);
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
                DM_SENDMSG(cloneMsg, client);
            }
        }
    }
    cloneMsg->unref();
}

void dm_load_clone(GameHost_Private* host, GameClient_Private* client,
                   MOUL::NetMsgLoadClone* netmsg)
{
    MOUL::LoadCloneMsg* msg = netmsg->m_message->Cast<MOUL::LoadCloneMsg>();
    if (msg->makeSafeForNet()) {
        if (netmsg->m_isPlayer) {
            host->m_clientMutex.lock();
            client->m_clientKey = netmsg->m_object;
            client->m_isLoaded = netmsg->m_isLoading;
            host->m_clientMutex.unlock();
        } else {
            auto it = host->m_clones.find(netmsg->m_object);
            if (it != host->m_clones.end()) {
                it->second->unref();
                // If, for some reason, the client decides to send a dupe (it can happen...)
                if (netmsg->m_isLoading) {
                    netmsg->ref();
                    host->m_clones[netmsg->m_object] = netmsg;
                } else
                    host->m_clones.erase(it);
            } else if (netmsg->m_isLoading) {
                netmsg->ref();
                host->m_clones[netmsg->m_object] = netmsg;
            }
        }
        dm_propagate(host, netmsg, client->m_clientInfo.m_PlayerId);

        // Any subsequent sends of these message are initial states
        netmsg->m_isInitialState = true;
    }
}

void dm_game_message(GameHost_Private* host, Game_PropagateMessage* msg)
{
    DS::BlobStream stream(msg->m_message);
    MOUL::NetMessage* netmsg = nullptr;
    try {
        netmsg = MOUL::Factory::Read<MOUL::NetMessage>(&stream);
    } catch (const MOUL::FactoryException&) {
        ST::printf(stderr, "[Game] Warning: Ignoring message: {04X}\n",
                   msg->m_messageType);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    } catch (const std::exception& ex) {
        // magickal code to print out the name of the offending plMessage
        MOUL::NetMsgGameMessage* gameMsg = netmsg->Cast<MOUL::NetMsgGameMessage>();
        if (gameMsg && gameMsg->m_message) {
            switch (gameMsg->m_message->type()) {
#define CREATABLE_TYPE(id, name) \
            case id: \
                ST::printf(stderr, "[Game] Exception reading " #name ": {}\n", ex.what()); \
                break;
#include "creatable_types.inl"
#undef CREATABLE_TYPE
            }
        } else {
            ST::printf(stderr, "[Game] Exception reading net message: {}\n",
                       ex.what());
        }
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }
    if (!stream.atEof()) {
        ST::printf(stderr, "[Game] Incomplete parse of {04X}\n", netmsg->type());
        netmsg->unref();
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }

    try {
        switch (msg->m_messageType) {
        case MOUL::ID_NetMsgPagingRoom:
            break;
        case MOUL::ID_NetMsgGameStateRequest:
            dm_send_state(host, msg->m_client);
            break;
        case MOUL::ID_NetMsgGameMessage:
            {
                MOUL::NetMsgGameMessage* gameMsg = netmsg->Cast<MOUL::NetMsgGameMessage>();
                gameMsg->m_message->m_bcastFlags |= MOUL::Message::e_NetNonLocal;
                if (msg->m_client->m_isAdmin) {
                    if (gameMsg->m_contentFlags & MOUL::NetMessage::e_RouteToAllPlayers)
                        dm_broadcast(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
                    else
                        dm_propagate(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
                } else if (gameMsg->m_message->makeSafeForNet())
                    dm_propagate(host, netmsg, msg->m_client->m_clientInfo.m_PlayerId);
            }
            break;
        case MOUL::ID_NetMsgGameMessageDirected:
            {
                MOUL::NetMsgGameMessageDirected* directedMsg =
                        netmsg->Cast<MOUL::NetMsgGameMessageDirected>();
                directedMsg->m_message->m_bcastFlags |= MOUL::Message::e_NetNonLocal;
                if (msg->m_client->m_isAdmin || directedMsg->m_message->makeSafeForNet())
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
            ST::printf(stderr, "[Game] Warning: Unhandled message: {04X}\n",
                       msg->m_messageType);
            break;
        }
    } catch (const DS::SockHup&) {
        // Client wasn't paying attention
    }
    netmsg->unref();
    SEND_REPLY(msg, DS::e_NetSuccess);
}

void dm_bcast_agesdl_hook(GameHost_Private* host)
{
    Game_AgeInfo info = s_ages[host->m_ageFilename];

    MOUL::NetMsgSDLStateBCast* bcast = MOUL::NetMsgSDLStateBCast::Create();
    bcast->m_contentFlags = MOUL::NetMessage::e_HasTimeSent
                            | MOUL::NetMessage::e_NeedsReliableSend;
    bcast->m_timestamp.setNow();
    bcast->m_isInitial = true;
    bcast->m_persistOnServer = true;
    bcast->m_isAvatar = false;
    bcast->m_object.m_location = MOUL::Location(info.m_seqPrefix, -2, MOUL::Location::e_BuiltIn);
    bcast->m_object.m_name = "AgeSDLHook";
    bcast->m_object.m_type = 1;  // SceneObject
    bcast->m_object.m_id = 1;
    bcast->m_sdlBlob = host->m_ageSdlHook.toBlob();

    dm_propagate(host, bcast, 0);
    bcast->unref();
}

void dm_local_sdl_update(GameHost_Private* host, Game_SdlMessage* msg)
{
    SDL::State vaultState;
    try {
        vaultState = SDL::State::FromBlob(msg->m_node.m_Blob_1);
    } catch (const std::exception& ex) {
        fprintf(stderr, "[SDL] Error parsing vault AgeSDL state: %s\n", ex.what());
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }

    if (!vaultState.descriptor()) {
        fputs("[SDL] Received an vault update for an AgeSDLHook using an invalid descriptor!\n",
              stderr);
        SEND_REPLY(msg, DS::e_NetInternalError);
        return;
    }

    host->m_ageSdlHook.merge(vaultState);
    host->m_ageSdlHook.merge(host->m_globalState);
    host->m_localState.merge(vaultState);
    msg->m_node.m_Blob_1 = host->m_localState.toBlob();

    Auth_NodeInfo sdlNode;
    AuthClient_Private fakeClient;
    sdlNode.m_client = &fakeClient;
    sdlNode.m_internal = true;
    sdlNode.m_node = std::move(msg->m_node);
    sdlNode.m_revision.clear();

    // The client that put this message is the auth daemon, and it is blocking on us.
    SEND_REPLY(msg, DS::e_NetSuccess);

    s_authChannel.putMessage(e_VaultUpdateNode, reinterpret_cast<void*>(&sdlNode));
    if (fakeClient.m_channel.getMessage().m_messageType != DS::e_NetSuccess)
        fputs("[Game] Error writing SDL node back to vault\n", stderr);

    dm_bcast_agesdl_hook(host);
}

void dm_global_sdl_update(GameHost_Private* host)
{
    host->m_ageSdlHook.merge(host->m_globalState);
    dm_bcast_agesdl_hook(host);
}

void dm_gameHost(GameHost_Private* host)
{
    for ( ;; ) {
        DS::FifoMessage msg { -1, nullptr };
        try {
            msg = host->m_channel.getMessage();
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
            case e_GameLocalSdlUpdate:
                dm_local_sdl_update(host, reinterpret_cast<Game_SdlMessage*>(msg.m_payload));
                break;
            case e_GameGlobalSdlUpdate:
                dm_global_sdl_update(host);
                break;
            default:
                /* Invalid message...  This shouldn't happen */
                ST::printf(stderr, "[Game] Invalid message ({}) in message queue\n",
                           msg.m_messageType);
                exit(1);
                break;
            }
        } catch (const std::exception& ex) {
            ST::printf(stderr, "[Game] Exception raised processing message: {}\n",
                       ex.what());
            if (msg.m_payload) {
                // Keep clients from blocking on a reply
                SEND_REPLY(reinterpret_cast<Game_ClientMessage*>(msg.m_payload),
                           DS::e_NetInternalError);
            }
        }
    }

    // This line should be unreachable
    DS_ASSERT(false);
}

GameHost_Private* start_game_host(uint32_t ageMcpId)
{
    PGconn* postgres = PQconnectdb(ST::format(
                    "host='{}' port='{}' user='{}' password='{}' dbname='{}'",
                    DS::Settings::DbHostname(), DS::Settings::DbPort(),
                    DS::Settings::DbUsername(), DS::Settings::DbPassword(),
                    DS::Settings::DbDbaseName()).c_str());
    if (PQstatus(postgres) != CONNECTION_OK) {
        ST::printf(stderr, "Error connecting to postgres: {}", PQerrorMessage(postgres));
        PQfinish(postgres);
        return nullptr;
    }

    DS::PGresultRef result = DS::PQexecVA(postgres,
            "SELECT \"AgeUuid\", \"AgeFilename\", \"AgeIdx\", \"SdlIdx\", \"Temporary\""
            "    FROM game.\"Servers\" WHERE idx=$1",
            ageMcpId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQ_PRINT_ERROR(postgres, SELECT);
        result.reset();
        PQfinish(postgres);
        return nullptr;
    }
    if (PQntuples(result) == 0) {
        ST::printf(stderr, "[Game] Age MCP {} not found\n", ageMcpId);
        result.reset();
        PQfinish(postgres);
        return nullptr;
    } else {
        if (PQntuples(result) != 1) {
            ST::printf(stderr, "[Game] WARNING: Multiple servers found for MCP {}\n",
                       ageMcpId);
        }

        GameHost_Private* host = new GameHost_Private();
        host->m_instanceId = PQgetvalue(result, 0, 0);
        host->m_ageFilename = PQgetvalue(result, 0, 1);
        host->m_ageIdx = strtoul(PQgetvalue(result, 0, 2), nullptr, 10);
        host->m_gameMaster = 0;
        host->m_serverIdx = ageMcpId;
        host->m_postgres = postgres;
        host->m_temp = strcmp("t", PQgetvalue(result, 0, 4)) == 0;

        // Fetch the age states
        Auth_FetchSDL sdlFetch;
        AuthClient_Private fakeClient;
        sdlFetch.m_client = &fakeClient;
        sdlFetch.m_ageFilename = host->m_ageFilename;
        sdlFetch.m_sdlNodeId = strtoul(PQgetvalue(result, 0, 3), nullptr, 10);
        result.reset();

        s_authChannel.putMessage(e_AuthFetchSDL, reinterpret_cast<void*>(&sdlFetch));
        DS::FifoMessage reply = fakeClient.m_channel.getMessage();
        if (reply.m_messageType != DS::e_NetSuccess) {
            fputs("[Game] Error fetching Age SDL\n", stderr);
            PQfinish(postgres);
            delete host;
            return nullptr;
        }
        host->m_sdlIdx = sdlFetch.m_sdlNodeId;
        host->m_globalState = sdlFetch.m_globalState;

        // Load the local state and see if it needs to be updated
        try {
            host->m_localState = SDL::State::FromBlob(sdlFetch.m_localState);
        } catch (const std::exception& ex) {
            ST::printf(stderr, "[SDL] Error parsing Age SDL state for {}: {}\n",
                       host->m_ageFilename, ex.what());
            PQfinish(postgres);
            delete host;
            return nullptr;
        }
        if (!host->m_localState.descriptor()) {
            // NULL VaultSDL Node (or it doesn't exist) -- maybe there is a descriptor now?
            SDL::StateDescriptor* desc = SDL::DescriptorDb::FindLatestDescriptor(host->m_ageFilename);
            if (desc) {
                host->m_localState = SDL::State(desc);
                DS::Blob local = host->m_localState.toBlob();
                host->m_ageSdlHook = SDL::State::FromBlob(local);
                dm_local_sdl_update(host, std::move(local));
            }
        } else if (host->m_localState.update()) {
            // The SDL Descriptor was updated
            DS::Blob local = host->m_localState.toBlob();
            host->m_ageSdlHook = SDL::State::FromBlob(local);
            dm_local_sdl_update(host, std::move(local));
        } else {
            // Perfectly valid old SDL Blob
            host->m_ageSdlHook = SDL::State::FromBlob(sdlFetch.m_localState);
        }
        host->m_ageSdlHook.merge(host->m_globalState);

        s_gameHostMutex.lock();
        s_gameHosts[ageMcpId] = host;
        s_gameHostMutex.unlock();

        // Fetch initial server state
        result = DS::PQexecVA(host->m_postgres,
                "SELECT \"Descriptor\", \"ObjectKey\", \"SdlBlob\""
                "    FROM game.\"AgeStates\" WHERE \"ServerIdx\"=$1",
                host->m_serverIdx);
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQ_PRINT_ERROR(host->m_postgres, SELECT);
        } else {
            int count = PQntuples(result);

            for (int i=0; i<count; ++i) {
                DS::Blob objblob = DS::Base64Decode(PQgetvalue(result, i, 1));
                DS::BlobStream bsObject(objblob);
                DS::Blob sdlblob = DS::Base64Decode(PQgetvalue(result, i, 2));
                MOUL::Uoid key;
                key.read(&bsObject);
                try {
                    SDL::State state = SDL::State::FromBlob(sdlblob);
                    state.update();

                    GameState gs;
                    gs.m_isAvatar = false;
                    gs.m_persist = true;
                    gs.m_state = state;
                    host->m_states[key][PQgetvalue(result, i, 0)] = gs;
                } catch (const std::exception& ex) {
                    ST::printf(stderr, "[SDL] Error parsing state {} for [{04X}]{}: {}\n",
                               PQgetvalue(result, i, 0), key.m_type, key.m_name,
                               ex.what());
                }
            }
        }

        std::thread threadh(&dm_gameHost, host);
        threadh.detach();
        return host;
    }
}
