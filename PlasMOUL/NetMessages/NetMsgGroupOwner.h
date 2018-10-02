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

#ifndef _MOUL_NETMSGGROUPOWNER_H
#define _MOUL_NETMSGGROUPOWNER_H

#include "NetMessage.h"
#include "NetGroupId.h"

struct stat;
namespace MOUL
{
    class NetMsgGroupOwner : public NetMsgServerToClient
    {
        FACTORY_CREATABLE(NetMsgGroupOwner)

    public:
        struct GroupInfo
        {
            NetGroupId m_group;
            bool m_own;
        };

        std::vector<GroupInfo> m_groups;

        void read(DS::Stream* stream) override;
        void write(DS::Stream* stream) const override;

    protected:
        NetMsgGroupOwner(uint16_t type) : NetMsgServerToClient(type) { }
    };
}

#endif
