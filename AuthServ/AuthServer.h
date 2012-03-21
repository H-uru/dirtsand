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
    void AuthServer_Init();
    void AuthServer_Add(SocketHandle client);
    void AuthServer_Shutdown();

    void AuthServer_DisplayClients();

    void AuthServer_AddAcct(DS::String, DS::String);

    class DbException : public std::exception
    {
    public:
        DbException() throw() { }
        virtual ~DbException() throw() { }

        virtual const char* what() const throw()
        { return "[DbException] Postgres error"; }
    };
}

#endif
