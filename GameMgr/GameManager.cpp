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

#include "GameManager_private.h"
#include "NetIO/MsgChannel.h"
#include "PlasMOUL/factory.h"
#include "errors.h"

#include <pthread.h>

pthread_t s_gameMgrThread;
DS::MsgChannel s_gameMgrChannel;

void* dm_gameManager(void*)
{
    for ( ;; ) {
        DS::FifoMessage msg = s_gameMgrChannel.getMessage();
        Game_ClientMessage* climsg = (Game_ClientMessage*)msg.m_payload;
        try {
            switch (msg.m_messageType) {
                case e_GameMgrShutdown:
                    return 0;
                case e_GameMgrMessage:
                    // TODO: dispatch the message
                    climsg->m_client->m_channel.putMessage(0);
                    break;
                default:
                    /* Invalid message...  This shouldn't happen */
                    DS_DASSERT(0);
                    break;
            }
        } catch (DS::AssertException ex) {
            if(climsg)
                climsg->m_client->m_channel.putMessage(0);
        }
    }
}

void DS::GameManager_Init()
{
    pthread_create(&s_gameMgrThread, 0, &dm_gameManager, 0);
}

void DS::GameManager_Shutdown()
{
    s_gameMgrChannel.putMessage(e_GameMgrShutdown, 0);
    pthread_join(s_gameMgrThread, 0);
}

void DS::GameManager_Message(GameClient_Private& client, DS::Blob msg)
{
    GameMgr_Message message;
    message.m_msg = msg;
    message.m_client = &client;
    s_gameMgrChannel.putMessage(e_GameMgrMessage, &message);

    // If it turns out we don't need to ever block here, we can get rid of this
    client.m_channel.getMessage();
}
