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

#ifndef _MOUL_NETMESSAGE_H
#define _MOUL_NETMESSAGE_H

#include "creatable.h"
#include "Types/UnifiedTime.h"
#include "Types/Uuid.h"
#include <vector>

#define NETMSG_PROTOCOL_MAJ  (12)
#define NETMSG_PROTOCOL_MIN  ( 6)

namespace MOUL
{
    class NetMessage : public Creatable
    {
    public:
        enum ContentFlags
        {
            e_HasTimeSent               = (1<<0),
            e_HasGameMsgReceivers       = (1<<1),
            e_EchoBackToSender          = (1<<2),
            e_RequestP2P                = (1<<3),
            e_AllowTimeOut              = (1<<4),
            e_IndirectMember            = (1<<5),
            e_PublicIPClient            = (1<<6),
            e_HasContext                = (1<<7),
            e_AskVaultForGameState      = (1<<8),
            e_HasTransactionID          = (1<<9),
            e_NewSDLState               = (1<<10),
            e_InitialAgeStateRequest    = (1<<11),
            e_HasPlayerID               = (1<<12),
            e_UseRelevanceRegions       = (1<<13),
            e_HasAcctUuid               = (1<<14),
            e_InterAgeRouting           = (1<<15),
            e_HasVersion                = (1<<16),
            e_IsSystemMessage           = (1<<17),
            e_NeedsReliableSend         = (1<<18),
            e_RouteToAllPlayers         = (1<<19),
        };

        uint32_t m_contentFlags;
        uint8_t m_protocolVerMaj, m_protocolVerMin;
        DS::UnifiedTime m_timestamp;
        uint32_t m_context, m_transId, m_playerId;
        DS::Uuid m_acctId;

        virtual void read(DS::Stream* stream);
        virtual void write(DS::Stream* stream) const;

    protected:
        NetMessage(uint16_t type)
            : Creatable(type), m_contentFlags(0), m_protocolVerMaj(NETMSG_PROTOCOL_MAJ),
              m_protocolVerMin(NETMSG_PROTOCOL_MIN), m_context(0), m_transId(0),
              m_playerId(0) { }
    };

    class NetMsgServerToClient : public NetMessage
    {
    protected:
        NetMsgServerToClient(uint16_t type) : NetMessage(type) { }
    };
}

#endif
