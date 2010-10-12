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

#include "MsgChannel.h"
#include "errors.h"

DS::MsgChannel::MsgChannel()
{
    int result;

    result = pthread_mutex_init(&m_queueMutex, 0);
    DS_PASSERT(result == 0);
    result = sem_init(&m_semaphore, 0, 0);
    DS_PASSERT(result == 0);
}

DS::MsgChannel::~MsgChannel()
{
    int result;

    result = sem_destroy(&m_semaphore);
    DS_PASSERT(result == 0);
    result = pthread_mutex_destroy(&m_queueMutex);
    DS_PASSERT(result == 0);
}

void DS::MsgChannel::putMessage(int type, void* payload)
{
    pthread_mutex_lock(&m_queueMutex);
    FifoMessage* msg = new FifoMessage;
    msg->m_messageType = type;
    msg->m_payload = payload;
    m_queue.push(msg);
    pthread_mutex_unlock(&m_queueMutex);
    sem_post(&m_semaphore);
}

DS::FifoMessage* DS::MsgChannel::getMessage()
{
    sem_wait(&m_semaphore);
    pthread_mutex_lock(&m_queueMutex);
    FifoMessage* msg = m_queue.front();
    m_queue.pop();
    pthread_mutex_unlock(&m_queueMutex);
    return msg;
}
