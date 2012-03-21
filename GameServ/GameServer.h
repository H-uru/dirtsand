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

#ifndef _DS_GAMESERVER_H
#define _DS_GAMESERVER_H

#include "NetIO/SockIO.h"
#include "Types/Uuid.h"
#include <exception>

namespace DS
{
    namespace Vault {
        class Node;
    }

    void GameServer_Init();
    void GameServer_Add(SocketHandle client);
    void GameServer_Shutdown();

    bool GameServer_UpdateVaultSDL(const DS::Vault::Node& node, uint32_t ageMcpId);

    void GameServer_DisplayClients();
    uint32_t GameServer_GetNumClients(Uuid instance);
}

#endif
