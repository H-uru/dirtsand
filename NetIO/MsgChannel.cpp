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

#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include "errors.h"

DS::MsgChannel::~MsgChannel()
{
    if (m_semaphore < 0)
        return;

    int result = close(m_semaphore);
    if (result < 0 && errno != EBADF) {
        ST::printf(stderr, "WARNING: Failed to close event semaphore: {}\n",
                   strerror(errno));
    }
}

int DS::MsgChannel::fd()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (m_semaphore >= 0)
        return m_semaphore;

    m_semaphore = eventfd(0, EFD_SEMAPHORE);
    if (m_semaphore < 0)
        throw SystemError("Failed to create event semaphore", strerror(errno));
    return m_semaphore;
}

void DS::MsgChannel::putMessage(int type, void* payload)
{
    FifoMessage msg;
    msg.m_messageType = type;
    msg.m_payload = payload;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_queue.push(msg);
    }

    int result = eventfd_write(fd(), 1);
    if (result < 0)
        throw SystemError("Failed to write to event semaphore", strerror(errno));
}

DS::FifoMessage DS::MsgChannel::getMessage()
{
    eventfd_t value;
    int result = eventfd_read(fd(), &value);
    if (result < 0)
        throw SystemError("Failed to read from event semaphore", strerror(errno));

    std::lock_guard<std::mutex> guard(m_mutex);
    FifoMessage msg = m_queue.front();
    m_queue.pop();
    return msg;
}

bool DS::MsgChannel::hasMessage()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return !m_queue.empty();
}
