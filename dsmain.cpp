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
#include "GameServ/GameServer.h"
#include "SDL/DescriptorDb.h"
#include "strings.h"
#include "errors.h"
#include "settings.h"
#include "encodings.h"
#include <readline.h>
#include <history.h>
#include <signal.h>
#include <execinfo.h>
#include <cstdio>

#ifdef DEBUG
extern bool s_commdebug;
#endif

static char** dup_strlist(const char* text, const char** strlist, size_t count)
{
    char** dupe;
    if (count == 1) {
        dupe = reinterpret_cast<char**>(malloc(sizeof(char*) * 2));
        dupe[0] = strdup(strlist[0]);
        dupe[1] = 0;
    } else {
        dupe = reinterpret_cast<char**>(malloc(sizeof(char*) * (count + 2)));
        dupe[0] = strdup(text);
        for (size_t i=0; i<count; ++i)
            dupe[i+1] = strdup(strlist[i]);
        dupe[count+1] = 0;
    }
    return dupe;
}

static char** console_completer(const char* text, int start, int end)
{
    static const char* completions[] = {
        /* Commands */
        "addacct", "clients", "commdebug", "help", "keygen", "quit", "restart",
        /* Services */
        "auth", "lobby",
    };

    rl_attempted_completion_over = true;
    if (strlen(text) == 0)
        return dup_strlist(text, completions, sizeof(completions) / sizeof(completions[0]));

    std::vector<const char*> matches;
    for (size_t i=0; i<sizeof(completions) / sizeof(completions[0]); ++i) {
        if (strncmp(text, completions[i], strlen(text)) == 0)
            matches.push_back(completions[i]);
    }
    if (matches.size() == 0)
        return 0;
    return dup_strlist(text, matches.data(), matches.size());
}

static void print_trace(const char* text)
{
    const size_t max_depth = 100;
    size_t stack_depth;
    void* stack_addrs[max_depth];
    char** stack_strings;

    stack_depth = backtrace(stack_addrs, max_depth);
    stack_strings = backtrace_symbols(stack_addrs, stack_depth);

    fprintf(stderr, "%s at %s\n", text, stack_strings[0]);
    for (size_t i=1; i<stack_depth; ++i)
        fprintf(stderr, "    from %s\n", stack_strings[i]);
    free(stack_strings);
}

static void sigh_segv(int)
{
    print_trace("Segfault");
    abort();
}

static void exception_filter()
{
    print_trace("Unhandled exception");
    abort();
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        fprintf(stderr, "Warning: No config file specified. Using defaults...\n");
        DS::Settings::UseDefaults();
    } else if (!DS::Settings::LoadFrom(argv[1])) {
        return 1;
    }

    // Show a stackdump in case we crash
    std::set_terminate(&exception_filter);
    signal(SIGSEGV, &sigh_segv);

    // Ignore sigpipe and force send() to return EPIPE
    signal(SIGPIPE, SIG_IGN);

    SDL::DescriptorDb::LoadDescriptors(DS::Settings::SdlPath());
    DS::FileServer_Init();
    DS::AuthServer_Init();
    DS::GameServer_Init();
    DS::GateKeeper_Init();
    DS::StartLobby();

    char* cmdbuf = 0;
    rl_attempted_completion_function = &console_completer;
    for ( ;; ) {
        cmdbuf = readline("");
        if (!cmdbuf)
            break;

        std::vector<DS::String> args = DS::String(cmdbuf).strip('#').split();
        if (args.size() == 0) {
            free(cmdbuf);
            continue;
        }
        add_history(cmdbuf);
        free(cmdbuf);

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
                } else if (*svc == "auth") {
                    DS::AuthServer_Shutdown();
                    DS::AuthServer_Init();
                    printf("Auth server restarted\n");
                } else {
                    fprintf(stderr, "Error: Service %s cannot be restarted\n", svc->c_str());
                }
            }
        } else if (args[0] == "keygen") {
            uint8_t xbuffer[64];
            if (args.size() != 2) {
                fprintf(stderr, "Please specify new, ue or pc\n");
            } else if (args[1] == "new") {
                uint8_t nbuffer[3][64], kbuffer[3][64];
                printf("Generating new server keys...  This will take a while.");
                fflush(stdout);
                for (size_t i=0; i<3; ++i)
                    DS::GenPrimeKeys(nbuffer[i], kbuffer[i]);

                printf("\n--------------------\n");
                printf("Server keys:\n");
                printf("Server.Auth.N = %s\n", DS::Base64Encode(nbuffer[0], 64).c_str());
                printf("Server.Auth.K = %s\n", DS::Base64Encode(kbuffer[0], 64).c_str());
                printf("Server.Game.N = %s\n", DS::Base64Encode(nbuffer[1], 64).c_str());
                printf("Server.Game.K = %s\n", DS::Base64Encode(kbuffer[1], 64).c_str());
                printf("Server.Gate.N = %s\n", DS::Base64Encode(nbuffer[2], 64).c_str());
                printf("Server.Gate.K = %s\n", DS::Base64Encode(kbuffer[2], 64).c_str());

                printf("--------------------\n");
                printf("UruExplorer:\n");
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
                printf("PlasmaClient:\n");
                DS::CryptCalcX(xbuffer, nbuffer[0], kbuffer[0], CRYPT_BASE_AUTH);
                printf("Server.Auth.N %s\n", DS::Base64Encode(nbuffer[0], 64).c_str());
                printf("Server.Auth.X %s\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, nbuffer[1], kbuffer[1], CRYPT_BASE_GAME);
                printf("Server.Game.N %s\n", DS::Base64Encode(nbuffer[1], 64).c_str());
                printf("Server.Game.X %s\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, nbuffer[2], kbuffer[2], CRYPT_BASE_GATE);
                printf("Server.Gate.N %s\n", DS::Base64Encode(nbuffer[2], 64).c_str());
                printf("Server.Gate.X %s\n", DS::Base64Encode(xbuffer, 64).c_str());
                printf("--------------------\n");
            } else if (args[1] == "ue") {
                uint8_t xbuffer[64];
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyAuth_N),
                               DS::Settings::CryptKey(DS::e_KeyAuth_K), CRYPT_BASE_AUTH);
                printf("Auth client keys:\n");
                DS::PrintClientKeys(xbuffer, DS::Settings::CryptKey(DS::e_KeyAuth_N));

                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGame_N),
                               DS::Settings::CryptKey(DS::e_KeyGame_K), CRYPT_BASE_GAME);
                printf("Game client keys:\n");
                DS::PrintClientKeys(xbuffer, DS::Settings::CryptKey(DS::e_KeyGame_N));

                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGate_N),
                               DS::Settings::CryptKey(DS::e_KeyGate_K), CRYPT_BASE_GATE);
                printf("GateKeeper client keys:\n");
                DS::PrintClientKeys(xbuffer, DS::Settings::CryptKey(DS::e_KeyGate_N));
            } else if (args[1] == "pc") {
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyAuth_N),
                               DS::Settings::CryptKey(DS::e_KeyAuth_K), CRYPT_BASE_AUTH);
                printf("Server.Auth.N %s\n", DS::Base64Encode(DS::Settings::CryptKey(DS::e_KeyAuth_N), 64).c_str());
                printf("Server.Auth.X %s\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGame_N),
                               DS::Settings::CryptKey(DS::e_KeyGame_K), CRYPT_BASE_GAME);
                printf("Server.Game.N %s\n", DS::Base64Encode(DS::Settings::CryptKey(DS::e_KeyGame_N), 64).c_str());
                printf("Server.Game.X %s\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGate_N),
                               DS::Settings::CryptKey(DS::e_KeyGate_K), CRYPT_BASE_GATE);
                printf("Server.Gate.N %s\n", DS::Base64Encode(DS::Settings::CryptKey(DS::e_KeyGate_N), 64).c_str());
                printf("Server.Gate.X %s\n", DS::Base64Encode(xbuffer, 64).c_str());
            } else {
                fprintf(stderr, "Error: %s is not a valid keygen target\n", args[1].c_str());
                continue;
            }
        } else if (args[0] == "clients") {
            DS::GateKeeper_DisplayClients();
            DS::FileServer_DisplayClients();
            DS::AuthServer_DisplayClients();
            DS::GameServer_DisplayClients();
        } else if (args[0] == "commdebug") {
#ifdef DEBUG
            if (args.size() == 1)
                printf("Comm debugging is currently %s\n", s_commdebug ? "on" : "off");
            else if (args[1] == "on")
                s_commdebug = true;
            else if (args[1] == "off")
                s_commdebug = false;
            else
                fprintf(stderr, "Error: must specify on or off\n");
#else
            fprintf(stderr, "Error: COMM debugging is only enabled in debug builds\n");
#endif
        } else if (args[0] == "addacct") {
            if (args.size() != 3)
                printf("Usage: addacct [user] [password]\n");
            DS::AuthServer_AddAcct(args[1], args[2]);
        } else if (args[0] == "help") {
            printf("DirtSand v1.0 Console supported commands:\n"
                   "    addacct [user] [password]\n"
                   "    clients\n"
                   "    commdebug <on|off>\n"
                   "    help\n"
                   "    keygen <new|ue|pc>\n"
                   "    quit\n"
                   "    restart <auth|lobby>\n"
                  );
        } else {
            fprintf(stderr, "Error: Unrecognized command: %s\n", args[0].c_str());
        }
    }

    DS::StopLobby();
    DS::GateKeeper_Shutdown();
    DS::GameServer_Shutdown();
    DS::AuthServer_Shutdown();
    DS::FileServer_Shutdown();
    return 0;
}
