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
#include "streams.h"
#include <pthread.h>
#include <list>

struct AuthServer_Private
{
    pthread_mutex_t m_mutex;

    DS::SocketHandle m_sock;
    DS::CryptState m_crypt;
    DS::BufferStream m_wkBuffer, m_dmBuffer;

    uint32_t m_challenge;
};

extern std::list<AuthServer_Private*> s_authClients;
extern pthread_mutex_t s_authClientMutex;
extern pthread_t s_authDaemonThread;

void* dm_authDaemon(void*);
void dm_auth_init();

enum AuthDaemonMessages
{
    e_AuthShutdown,
};
void AuthDaemon_SendMessage(int msg, void* data = 0);
