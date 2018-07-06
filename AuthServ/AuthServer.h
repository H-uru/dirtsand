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

#ifndef _DS_AUTHSERVER_H
#define _DS_AUTHSERVER_H

#include "NetIO/SockIO.h"
#include <exception>

namespace DS
{
    void AuthServer_Init(bool restrictLogins=false);
    void AuthServer_Add(SocketHandle client);
    bool AuthServer_RestrictLogins();
    void AuthServer_Shutdown();

    void AuthServer_DisplayClients();

    bool AuthServer_AddAcct(const ST::string&, const ST::string&);
    uint32_t AuthServer_AcctFlags(const ST::string& acctName, uint32_t flags);
    bool AuthServer_AddAllPlayersFolder(uint32_t playerId);
    bool AuthServer_ChangeGlobalSDL(const ST::string& ageName, const ST::string& var,
                                    const ST::string& value);
}

#endif
