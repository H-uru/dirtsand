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

#include "Lobby.h"
#include "GateKeeper/GateServ.h"
#include "FileServ/FileServer.h"
#include "AuthServ/AuthServer.h"
#include "GameServ/GameServer.h"
#include "Types/Uuid.h"
#include "SockIO.h"
#include "errors.h"
#include "settings.h"
#include <thread>
#include <cstdio>

enum ConnType
{
    e_ConnCliToAuth = 10,
    e_ConnCliToGame = 11,
    e_ConnCliToFile = 16,
    e_ConnCliToCsr = 20,
    e_ConnCliToGateKeeper = 22,
};

struct ConnectionHeader
{
    uint8_t m_connType;
    uint16_t m_sockHeaderSize;
    uint32_t m_buildId, m_buildType, m_branchId;
    DS::Uuid m_productId;
};

static std::thread s_lobbyThread;
static DS::SocketHandle s_listenSock;

void dm_lobby()
{
    ST::printf("[Lobby] Running on {}\n", DS::SockIpAddress(s_listenSock));
    {
        for ( ;; ) {
            DS::SocketHandle client;
            try {
                client = DS::AcceptSock(s_listenSock);
            } catch (const DS::SockHup&) {
                break;
            }

            if (!client)
                continue;

            try {
                ConnectionHeader header;
                header.m_connType = DS::RecvValue<uint8_t>(client);
                header.m_sockHeaderSize = DS::RecvValue<uint16_t>(client);
                header.m_buildId = DS::RecvValue<uint32_t>(client);
                header.m_buildType = DS::RecvValue<uint32_t>(client);
                header.m_branchId = DS::RecvValue<uint32_t>(client);
                DS::RecvBuffer(client, header.m_productId.m_bytes,
                               sizeof(header.m_productId.m_bytes));

                switch (header.m_connType) {
                case e_ConnCliToGateKeeper:
                    DS::GateKeeper_Add(client);
                    break;
                case e_ConnCliToFile:
                    DS::FileServer_Add(client);
                    break;
                case e_ConnCliToAuth:
                    DS::AuthServer_Add(client);
                    break;
                case e_ConnCliToGame:
                    DS::GameServer_Add(client);
                    break;
                case e_ConnCliToCsr:
                    ST::printf("[Lobby] {} - CSR client?  Get that mutha outta here!\n",
                               DS::SockIpAddress(client));
                    DS::FreeSock(client);
                    break;
                default:
                    ST::printf("[Lobby] {} - Unknown connection type!  Abandon ship!\n",
                           DS::SockIpAddress(client));
                    DS::FreeSock(client);
                    break;
                }
            } catch (const DS::SockHup& hup) {
                DS::FreeSock(client);
            } catch (const std::exception& ex) {
                ST::printf(stderr, "[Lobby] {} - Exception while processing incoming client: {}\n",
                           DS::SockIpAddress(client), ex.what());
                DS::FreeSock(client);
            }
        }
    }

    DS::FreeSock(s_listenSock);
}

void DS::StartLobby()
{
    try {
        s_listenSock = DS::BindSocket(DS::Settings::LobbyAddress(),
                                      DS::Settings::LobbyPort());
        DS::ListenSock(s_listenSock);
    } catch (const SystemError &err) {
        fputs(err.what(), stderr);
        exit(1);
    }
    s_lobbyThread = std::thread(&dm_lobby);
}

void DS::StopLobby()
{
    DS::CloseSock(s_listenSock);
    s_lobbyThread.join();
}
