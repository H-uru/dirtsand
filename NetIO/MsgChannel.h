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

#ifndef _DS_MSGCHANNEL_H
#define _DS_MSGCHANNEL_H

#include <queue>
#include <mutex>

namespace DS
{
    struct FifoMessage
    {
        int m_messageType;
        void* m_payload;
    };

    class MsgChannel
    {
    public:
        MsgChannel();
        ~MsgChannel();

        int fd() const { return m_semaphore; }
        void putMessage(int type, void* payload = 0);
        FifoMessage getMessage();
        bool hasMessage();

    private:
        int m_semaphore;
        std::mutex m_queueMutex;
        std::queue<FifoMessage> m_queue;
    };
}

#endif
