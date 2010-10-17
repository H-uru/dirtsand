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

#include "NetIO/Lobby.h"
#include "NetIO/CryptIO.h"
#include "GateKeeper/GateServ.h"
#include "FileServ/FileServer.h"
#include "AuthServ/AuthServer.h"
#include "strings.h"
#include "errors.h"
#include "settings.h"
#include "encodings.h"
#include <signal.h>
#include <cstdio>

#ifdef DEBUG
extern bool s_commdebug;
#endif

int main(int argc, char* argv[])
{
    if (argc == 1) {
        fprintf(stderr, "Warning: No config file specified. Using defaults...\n");
        DS::Settings::UseDefaults();
    } else if (!DS::Settings::LoadFrom(argv[1])) {
        return 1;
    }

    // Ignore sigpipe and force send() to return EPIPE
    signal(SIGPIPE, SIG_IGN);

    DS::FileServer_Init();
    DS::AuthServer_Init();
    DS::GateKeeper_Init();
    DS::StartLobby();

    char cmdbuf[4096];
    while (fgets(cmdbuf, 4096, stdin)) {
        std::vector<DS::String> args = DS::String(cmdbuf).strip('#').split();
        if (args.size() == 0)
            continue;

        if (args[0] == "quit") {
            break;
        } else if (args[0] == "restart") {
            if (args.size() < 2) {
                fprintf(stderr, "Error: No services specified\n");
                continue;
            }
            for (std::vector<DS::String>::iterator svc = args.begin() + 1;
                 svc != args.end(); ++svc) {
                if (*svc == "lobby") {
                    DS::StopLobby();
                    DS::StartLobby();
                } else {
                    fprintf(stderr, "Error: Service %s cannot be restarted\n", svc->c_str());
                }
            }
        } else if (args[0] == "keygen") {
            uint8_t xbuffer[64];
            if (args.size() == 1) {
                uint8_t nbuffer[3][64], kbuffer[3][64];
                printf("Generating new server keys...  This may take a while.");
                fflush(stdout);
                for (size_t i=0; i<3; ++i)
                    DS::GenPrimeKeys(nbuffer[i], kbuffer[i]);

                printf("\n--------------------\n");
                printf("Server keys:\n");
                printf("Key.Auth.N = %s\n", DS::Base64Encode(nbuffer[0], 64).c_str());
                printf("Key.Auth.K = %s\n", DS::Base64Encode(kbuffer[0], 64).c_str());
                printf("Key.Game.N = %s\n", DS::Base64Encode(nbuffer[1], 64).c_str());
                printf("Key.Game.K = %s\n", DS::Base64Encode(kbuffer[1], 64).c_str());
                printf("Key.Gate.N = %s\n", DS::Base64Encode(nbuffer[2], 64).c_str());
                printf("Key.Gate.K = %s\n", DS::Base64Encode(kbuffer[2], 64).c_str());

                printf("Auth client keys:\n");
                DS::CryptCalcX(xbuffer, nbuffer[0], kbuffer[0], CRYPT_BASE_AUTH);
                DS::PrintClientKeys(xbuffer, nbuffer[0]);

                printf("Game client keys:\n");
                DS::CryptCalcX(xbuffer, nbuffer[1], kbuffer[1], CRYPT_BASE_GAME);
                DS::PrintClientKeys(xbuffer, nbuffer[1]);

                printf("GateKeeper client keys:\n");
                DS::CryptCalcX(xbuffer, nbuffer[2], kbuffer[2], CRYPT_BASE_GATE);
                DS::PrintClientKeys(xbuffer, nbuffer[2]);
                printf("--------------------\n");
            } else if (args[1] == "auth") {
                uint8_t xbuffer[64];
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyAuth_N),
                               DS::Settings::CryptKey(DS::e_KeyAuth_K), CRYPT_BASE_AUTH);
                printf("Auth client keys:\n");
                DS::PrintClientKeys(xbuffer, DS::Settings::CryptKey(DS::e_KeyAuth_N));
            } else if (args[1] == "game") {
                uint8_t xbuffer[64];
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGame_N),
                               DS::Settings::CryptKey(DS::e_KeyGame_K), CRYPT_BASE_GAME);
                printf("Game client keys:\n");
                DS::PrintClientKeys(xbuffer, DS::Settings::CryptKey(DS::e_KeyGame_N));
            } else if (args[1] == "gate") {
                uint8_t xbuffer[64];
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGate_N),
                               DS::Settings::CryptKey(DS::e_KeyGate_K), CRYPT_BASE_GATE);
                printf("GateKeeper client keys:\n");
                DS::PrintClientKeys(xbuffer, DS::Settings::CryptKey(DS::e_KeyGate_N));
            } else {
                fprintf(stderr, "Error: %s is not a valid key type\n", args[1].c_str());
                continue;
            }
        } else if (args[0] == "commdebug") {
#ifdef DEBUG
            if (args.size() != 2)
                fprintf(stderr, "Error: Must specify on or off\n");
            else if (args[1] == "on")
                s_commdebug = true;
            else if (args[1] == "off")
                s_commdebug = false;
            else
                fprintf(stderr, "Error: must specify on or off\n");
#else
            fprintf(stderr, "Error: COMM debugging is only enabled in debug builds\n");
#endif
        } else if (args[0] == "help") {
            printf("DirtSand v1.0 Console supported commands:\n"
                   "    commdebug <on|off>\n"
                   "    help\n"
                   "    keygen [auth|game|gate]\n"
                   "    quit\n"
                   "    restart <auth|lobby>\n"
                  );
        } else {
            fprintf(stderr, "Error: Unrecognized command: %s\n", args[0].c_str());
        }
    }

    DS::StopLobby();
    DS::GateKeeper_Shutdown();
    DS::AuthServer_Shutdown();
    DS::FileServer_Shutdown();
    return 0;
}