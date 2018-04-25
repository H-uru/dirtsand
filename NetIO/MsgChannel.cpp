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

#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include "MsgChannel.h"

DS::MsgChannel::MsgChannel()
{
    m_semaphore = eventfd(0, EFD_SEMAPHORE);
    if (m_semaphore < 0) {
        fprintf(stderr, "FATAL: Failed to create event semaphore: %s\n",
                strerror(errno));
        exit(1);
    }
}

DS::MsgChannel::~MsgChannel()
{
    int result = close(m_semaphore);
    if (result < 0 && errno != EBADF) {
        fprintf(stderr, "WARNING: Failed to close event semaphore: %s\n",
                strerror(errno));
    }
}

void DS::MsgChannel::putMessage(int type, void* payload)
{
    m_queueMutex.lock();
    FifoMessage msg;
    msg.m_messageType = type;
    msg.m_payload = payload;
    m_queue.push(msg);
    m_queueMutex.unlock();

    int result = eventfd_write(m_semaphore, 1);
    if (result < 0) {
        fprintf(stderr, "FATAL: Failed to write to event semaphore: %s\n",
                strerror(errno));
        exit(1);
    }
}

DS::FifoMessage DS::MsgChannel::getMessage()
{
    eventfd_t value;
    int result = eventfd_read(m_semaphore, &value);
    if (result < 0) {
        fprintf(stderr, "FATAL: Failed to read from event semaphore: %s\n",
                strerror(errno));
        exit(1);
    }

    m_queueMutex.lock();
    FifoMessage msg = m_queue.front();
    m_queue.pop();
    m_queueMutex.unlock();
    return msg;
}

bool DS::MsgChannel::hasMessage()
{
    std::lock_guard<std::mutex> guard(m_queueMutex);
    return !m_queue.empty();
}
