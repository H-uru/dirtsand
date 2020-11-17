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

#include "Status.h"
#include "SockIO.h"
#include "errors.h"
#include "settings.h"
#include <cstdio>
#include <list>
#include <thread>
#include <string_theory/format>

static std::thread s_httpThread;
static DS::SocketHandle s_listenSock;

#define SEND_RAW(sock, str) \
    DS::SendBuffer((sock), static_cast<const void*>(str), strlen(str))

void dm_htserv()
{
    ST::printf("[Status] Running on {}\n", DS::SockIpAddress(s_listenSock));
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
            std::list<ST::string> lines;
            for ( ;; ) {
                std::unique_ptr<char[]> buffer;
                ST::string scratch;
                try {
                    size_t bufSize = DS::PeekSize(client);
                    buffer.reset(new char[scratch.size() + bufSize + 1]);
                    memcpy(buffer.get(), scratch.c_str(), scratch.size());
                    DS::RecvBuffer(client, buffer.get() + scratch.size(), bufSize);
                    buffer[scratch.size() + bufSize] = 0;
                } catch (const DS::SockHup&) {
                    lines.clear();
                    break;
                }

                char* cp = buffer.get();
                char* sp = buffer.get();
                while (*cp) {
                    if (*cp == '\r' || *cp == '\n') {
                        if (*cp == '\r' && *(cp + 1) == '\n') {
                            // Delete both chars of the Windows newline
                            *cp++ = 0;
                        }
                        *cp++ = 0;
                        lines.push_back(ST::string::from_latin_1(sp));
                        sp = cp;
                        scratch.clear();
                    } else {
                        ++cp;
                    }
                }
                if (cp != sp)
                    scratch += sp;

                if (lines.size() && lines.back().empty()) {
                    // Got the separator line
                    break;
                }
            }

            if (lines.empty()) {
                DS::FreeSock(client);
                continue;
            }

            // First line contains the action
            std::vector<ST::string> action = lines.front().tokenize();
            if (action.size() < 2) {
                ST::printf(stderr, "[Status] Incorrectly formatted HTTP request: {}\n",
                           lines.front());
                DS::FreeSock(client);
                continue;
            }
            if (action[0] != "GET") {
                ST::printf(stderr, "[Status] Unsupported method: {}\n", action[0]);
                DS::FreeSock(client);
                continue;
            }
            if (action.size() < 3 || action[2] != "HTTP/1.1") {
                fputs("[Status] Unsupported HTTP Version\n", stderr);
                DS::FreeSock(client);
                continue;
            }
            ST::string path = action[1];
            lines.pop_front();

            for (auto lniter = lines.begin(); lniter != lines.end(); ++lniter) {
                // TODO: See if any of these fields are useful to us...
            }

            if (path == "/status") {
                ST::string json = ST_LITERAL("{'online':true");
                ST::string welcome = DS::Settings::WelcomeMsg();
                welcome = welcome.replace("\"", "\\\"");
                json += ST::format(",'welcome':\"{}\"", welcome);
                json += "}\r\n";
                // TODO: Add more status fields (players/ages, etc)

                SEND_RAW(client, "HTTP/1.1 200 OK\r\n");
                SEND_RAW(client, "Server: Dirtsand\r\n");
                SEND_RAW(client, "Connection: close\r\n");
                SEND_RAW(client, "Accept-Ranges: bytes\r\n");
                ST::string lengthParam = ST::format("Content-Length: {}\r\n", json.size());
                DS::SendBuffer(client, lengthParam.c_str(), lengthParam.size());
                SEND_RAW(client, "Content-Type: application/json\r\n");
                SEND_RAW(client, "\r\n");
                DS::SendBuffer(client, json.c_str(), json.size());
                DS::FreeSock(client);
            } else if (path == "/welcome") {
                ST::string welcome = DS::Settings::WelcomeMsg();
                welcome = welcome.replace("\\n", "\r\n");

                SEND_RAW(client, "HTTP/1.1 200 OK\r\n");
                SEND_RAW(client, "Server: Dirtsand\r\n");
                SEND_RAW(client, "Connection: close\r\n");
                SEND_RAW(client, "Accept-Ranges: bytes\r\n");
                ST::string lengthParam = ST::format("Content-Length: {}\r\n", welcome.size());
                DS::SendBuffer(client, lengthParam.c_str(), lengthParam.size());
                SEND_RAW(client, "Content-Type: text/plain\r\n");
                SEND_RAW(client, "\r\n");
                DS::SendBuffer(client, welcome.c_str(), welcome.size());
                DS::FreeSock(client);
            } else {
                ST::string content = ST::format("No page found at {}\r\n", path);

                SEND_RAW(client, "HTTP/1.1 404 NOT FOUND\r\n");
                SEND_RAW(client, "Server: Dirtsand\r\n");
                SEND_RAW(client, "Connection: close\r\n");
                SEND_RAW(client, "Accept-Ranges: bytes\r\n");
                ST::string lengthParam = ST::format("Content-Length: {}\r\n", content.size());
                DS::SendBuffer(client, lengthParam.c_str(), lengthParam.size());
                SEND_RAW(client, "Content-Type: text/plain\r\n");
                SEND_RAW(client, "\r\n");
                DS::SendBuffer(client, content.c_str(), content.size());
                DS::FreeSock(client);
            }
        } catch (const std::exception& ex) {
            ST::printf(stderr, "[Status] Exception occurred serving HTTP to {}: {}\n",
                       DS::SockIpAddress(client), ex.what());
            DS::FreeSock(client);
        }
    }

    DS::FreeSock(s_listenSock);
}

void DS::StartStatusHTTP()
{
    try {
        s_listenSock = DS::BindSocket(DS::Settings::StatusAddress(),
                                      DS::Settings::StatusPort());
        DS::ListenSock(s_listenSock);
    } catch (const SystemError &err) {
        ST::printf(stderr, "[Status] WARNING: Could not start the HTTP server: {}\n",
                   err.what());
        return;
    }
    s_httpThread = std::thread(&dm_htserv);
}

void DS::StopStatusHTTP()
{
    if (s_httpThread.joinable()) {
        DS::CloseSock(s_listenSock);
        s_httpThread.join();
    }
}
