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

#include "MsgChannel.h"
#include "errors.h"

DS::MsgChannel::MsgChannel()
{
    int result;

    result = sem_init(&m_semaphore, 0, 0);
    DS_PASSERT(result == 0);
}

DS::MsgChannel::~MsgChannel()
{
    int result;

    result = sem_destroy(&m_semaphore);
    DS_PASSERT(result == 0);
}

void DS::MsgChannel::putMessage(int type, void* payload)
{
    m_queueMutex.lock();
    FifoMessage msg;
    msg.m_messageType = type;
    msg.m_payload = payload;
    m_queue.push(msg);
    m_queueMutex.unlock();
    sem_post(&m_semaphore);
}

DS::FifoMessage DS::MsgChannel::getMessage()
{
    sem_wait(&m_semaphore);
    m_queueMutex.lock();
    FifoMessage msg = m_queue.front();
    m_queue.pop();
    m_queueMutex.unlock();
    return msg;
}
