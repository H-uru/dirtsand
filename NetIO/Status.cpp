/******************************************************************************
 * This file is part of dirtsand.                                             *
 *                                                                            *
 * dirtsand is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation, either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * dirtsand is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.          *
 ******************************************************************************/

#include "Status.h"
#include "SockIO.h"
#include "errors.h"
#include "settings.h"
#include "strings.h"
#include <cstdio>
#include <list>
#include <thread>

static std::thread s_httpThread;
static DS::SocketHandle s_listenSock;

#define SEND_RAW(sock, str) \
    DS::SendBuffer((sock), static_cast<const void*>(str), strlen(str))

void dm_htserv()
{
    printf("[Status] Running on %s\n", DS::SockIpAddress(s_listenSock).c_str());
    try {
        for ( ;; ) {
            DS::SocketHandle client;
            try {
                client = DS::AcceptSock(s_listenSock);
            } catch (DS::SockHup) {
                break;
            }

            if (!client)
                continue;

            std::list<DS::String> lines;
            for ( ;; ) {
                char* buffer = 0;
                DS::String scratch;
                try {
                    size_t bufSize = DS::PeekSize(client);
                    buffer = new char[scratch.length() + bufSize + 1];
                    memcpy(buffer, scratch.c_str(), scratch.length());
                    DS::RecvBuffer(client, buffer + scratch.length(), bufSize);
                    buffer[scratch.length() + bufSize] = 0;
                } catch (DS::SockHup) {
                    lines.clear();
                    delete[] buffer;
                    break;
                }

                char* cp = buffer;
                char* sp = buffer;
                while (*cp) {
                    if (*cp == '\r' || *cp == '\n') {
                        if (*cp == '\r' && *(cp + 1) == '\n') {
                            // Delete both chars of the Windows newline
                            *cp++ = 0;
                        }
                        *cp++ = 0;
                        lines.push_back(DS::String(sp));
                        sp = cp;
                        scratch = "";
                    } else {
                        ++cp;
                    }
                }
                if (cp != sp)
                    scratch += sp;
                delete[] buffer;

                if (lines.size() && lines.back().isEmpty()) {
                    // Got the separator line
                    break;
                }
            }

            if (lines.empty()) {
                DS::FreeSock(client);
                continue;
            }

            // First line contains the action
            std::vector<DS::String> action = lines.front().split();
            if (action.size() < 2) {
                fprintf(stderr, "[Status] Incorrectly formatted HTTP request: %s\n",
                        lines.front().c_str());
                DS::FreeSock(client);
                continue;
            }
            if (action[0] != "GET") {
                fprintf(stderr, "[Status] Unsupported method: %s\n", action[0].c_str());
                DS::FreeSock(client);
                continue;
            }
            if (action.size() < 3 || action[2] != "HTTP/1.1") {
                fprintf(stderr, "[Status] Unsupported HTTP Version\n");
                DS::FreeSock(client);
                continue;
            }
            DS::String path = action[1];
            lines.pop_front();

            for (auto lniter = lines.begin(); lniter != lines.end(); ++lniter) {
                // TODO: See if any of these fields are useful to us...
            }

            if (path == "/status") {
                DS::String json = "{'online':true";
                DS::String welcome = DS::Settings::WelcomeMsg();
                welcome.replace("\"", "\\\"");
                json += DS::String::Format(",'welcome':\"%s\"", welcome.c_str());
                json += "}\r\n";
                // TODO: Add more status fields (players/ages, etc)

                SEND_RAW(client, "HTTP/1.1 200 OK\r\n");
                SEND_RAW(client, "Server: Dirtsand\r\n");
                SEND_RAW(client, "Connection: close\r\n");
                SEND_RAW(client, "Accept-Ranges: bytes\r\n");
                DS::String lengthParam = DS::String::Format("Content-Length: %u\r\n", json.length());
                DS::SendBuffer(client, lengthParam.c_str(), lengthParam.length());
                SEND_RAW(client, "Content-Type: application/json\r\n");
                SEND_RAW(client, "\r\n");
                DS::SendBuffer(client, json.c_str(), json.length());
                DS::FreeSock(client);
            } else if (path == "/welcome") {
                DS::String welcome = DS::Settings::WelcomeMsg();
                welcome.replace("\\n", "\r\n");

                SEND_RAW(client, "HTTP/1.1 200 OK\r\n");
                SEND_RAW(client, "Server: Dirtsand\r\n");
                SEND_RAW(client, "Connection: close\r\n");
                SEND_RAW(client, "Accept-Ranges: bytes\r\n");
                DS::String lengthParam = DS::String::Format("Content-Length: %u\r\n", welcome.length());
                DS::SendBuffer(client, lengthParam.c_str(), lengthParam.length());
                SEND_RAW(client, "Content-Type: text/plain\r\n");
                SEND_RAW(client, "\r\n");
                DS::SendBuffer(client, welcome.c_str(), welcome.length());
                DS::FreeSock(client);
            } else {
                DS::String content = DS::String::Format("No page found at %s\r\n", path.c_str());

                SEND_RAW(client, "HTTP/1.1 404 NOT FOUND\r\n");
                SEND_RAW(client, "Server: Dirtsand\r\n");
                SEND_RAW(client, "Connection: close\r\n");
                SEND_RAW(client, "Accept-Ranges: bytes\r\n");
                DS::String lengthParam = DS::String::Format("Content-Length: %u\r\n", content.length());
                DS::SendBuffer(client, lengthParam.c_str(), lengthParam.length());
                SEND_RAW(client, "Content-Type: text/plain\r\n");
                SEND_RAW(client, "\r\n");
                DS::SendBuffer(client, content.c_str(), content.length());
                DS::FreeSock(client);
            }
        }
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[Status] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
    }

    DS::FreeSock(s_listenSock);
}

void DS::StartStatusHTTP()
{
    s_listenSock = DS::BindSocket(DS::Settings::StatusAddress(),
                                  DS::Settings::StatusPort());
    DS::ListenSock(s_listenSock);
    s_httpThread = std::thread(&dm_htserv);
}

void DS::StopStatusHTTP()
{
    DS::CloseSock(s_listenSock);
    s_httpThread.join();
}
