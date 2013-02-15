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

#include "factory.h"
#include "errors.h"

#include "AgeLinkStruct.h"
#include "Avatar/AvBrainCoop.h"
#include "Avatar/AvBrainGeneric.h"
#include "Avatar/CoopCoordinator.h"
#include "Messages/LoadAvatarMsg.h"
#include "Messages/AvatarInputStateMsg.h"
#include "Messages/ServerReplyMsg.h"
#include "Messages/NotifyMsg.h"
#include "Messages/InputIfaceMgrMsg.h"
#include "Messages/ClothingMsg.h"
#include "Messages/LinkEffectsTriggerMsg.h"
#include "Messages/KIMessage.h"
#include "Messages/AvSeekMsg.h"
#include "Messages/EnableMsg.h"
#include "Messages/BulletMsg.h"
#include "Messages/SimulationMsg.h"
#include "Messages/MessageWithCallbacks.h"
#include "Messages/LinkToAgeMsg.h"
#include "Messages/ParticleMsg.h"
#include "Messages/MultistageMsg.h"
#include "Messages/SetNetGroupIdMsg.h"
#include "Messages/InputEventMsg.h"
#include "NetMessages/NetMsgLoadClone.h"
#include "NetMessages/NetMsgPlayerPage.h"
#include "NetMessages/NetMsgMembersList.h"
#include "NetMessages/NetMsgGameState.h"
#include "NetMessages/NetMsgSharedState.h"
#include "NetMessages/NetMsgSDLState.h"
#include "NetMessages/NetMsgGroupOwner.h"
#include "NetMessages/NetMsgRelevanceRegions.h"
#include "NetMessages/NetMsgVoice.h"

MOUL::Creatable* MOUL::Factory::Create(uint16_t type)
{
    switch (type) {
/* THAR BE MAJICK HERE */
#define CREATABLE_TYPE(id, cre) \
    case id: return new cre(id);
#include "creatable_types.inl"
#undef CREATABLE_TYPE
    case 0x8000: return static_cast<Creatable*>(0);
    default:
        fprintf(stderr, "[Factory] Tried to create unknown type %04X\n", type);
        throw FactoryException(FactoryException::e_UnknownType);
    }
}

MOUL::Creatable* MOUL::Factory::ReadCreatable(DS::Stream* stream)
{
    uint16_t type = stream->read<uint16_t>();
    Creatable* obj = Create(type);
    if (obj) {
        try {
            obj->read(stream);
        } catch (...) {
            obj->unref();
            throw;
        }
    }
    return obj;
}

void MOUL::Factory::WriteCreatable(DS::Stream* stream, MOUL::Creatable* obj)
{
    if (obj) {
        stream->write<uint16_t>(obj->type());
        obj->write(stream);
    } else {
        stream->write<uint16_t>(0x8000);
    }
}
