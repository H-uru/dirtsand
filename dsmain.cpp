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

#include "NetIO/Lobby.h"
#include "NetIO/Status.h"
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
#include <unistd.h>
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
        "welcome",
        /* Services */
        "auth", "lobby", "status",
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

static void sigh_term(int)
{
    fclose(stdin);
}

int main(int argc, char* argv[])
{
    // refuse to run as root
    if (geteuid() == 0) {
        fputs("Do not run this server as root!\n", stderr);
        return 1;
    }

    if (argc == 1) {
        fputs("Warning: No config file specified. Using defaults...\n", stderr);
        DS::Settings::UseDefaults();
    } else if (!DS::Settings::LoadFrom(argv[1])) {
        return 1;
    }

    // Show a stackdump in case we crash
    std::set_terminate(&exception_filter);
    signal(SIGSEGV, &sigh_segv);
    signal(SIGTERM, &sigh_term);

    // Ignore sigpipe and force send() to return EPIPE
    signal(SIGPIPE, SIG_IGN);

    SDL::DescriptorDb::LoadDescriptors(DS::Settings::SdlPath());
    DS::FileServer_Init();
    DS::AuthServer_Init();
    DS::GameServer_Init();
    DS::GateKeeper_Init();
    DS::StartLobby();
    DS::StartStatusHTTP();

    char rl_prompt[32];
    snprintf(rl_prompt, 32, "ds-%u> ", CLIENT_BUILD_ID);

    char* cmdbuf = 0;
    rl_attempted_completion_function = &console_completer;
    for ( ;; ) {
        cmdbuf = readline(rl_prompt);
        if (!cmdbuf) {
            // Get us out of the prompt
            fputc('\n', stdout);
            break;
        }

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
                fputs("Error: No services specified\n", stderr);
                continue;
            }
            for (auto svc = args.begin() + 1; svc != args.end(); ++svc) {
                if (*svc == "lobby") {
                    DS::StopLobby();
                    DS::StartLobby();
                } else if (*svc == "auth") {
                    DS::AuthServer_Shutdown();
                    DS::AuthServer_Init();
                    fputs("Auth server restarted\n", stdout);
                } else if (*svc == "status") {
                    DS::StopStatusHTTP();
                    DS::StartStatusHTTP();
                } else {
                    fprintf(stderr, "Error: Service %s cannot be restarted\n", svc->c_str());
                }
            }
        } else if (args[0] == "keygen") {
            uint8_t xbuffer[64];
            if (args.size() != 2) {
                fputs("Usage:  keygen <new|show>\n", stderr);
            } else if (args[1] == "new") {
                uint8_t nbuffer[3][64], kbuffer[3][64];
                fputs("Generating new server keys...  This will take a while.", stdout);
                fflush(stdout);
                for (size_t i=0; i<3; ++i)
                    DS::GenPrimeKeys(nbuffer[i], kbuffer[i]);

                fputs("\n--------------------\n", stdout);
                fputs("Server keys: (dirtsand.ini)\n", stdout);
                printf("Key.Auth.N = %s\n", DS::Base64Encode(nbuffer[0], 64).c_str());
                printf("Key.Auth.K = %s\n", DS::Base64Encode(kbuffer[0], 64).c_str());
                printf("Key.Game.N = %s\n", DS::Base64Encode(nbuffer[1], 64).c_str());
                printf("Key.Game.K = %s\n", DS::Base64Encode(kbuffer[1], 64).c_str());
                printf("Key.Gate.N = %s\n", DS::Base64Encode(nbuffer[2], 64).c_str());
                printf("Key.Gate.K = %s\n", DS::Base64Encode(kbuffer[2], 64).c_str());

                fputs("--------------------\n", stdout);
                fputs("Client keys: (server.ini)\n", stdout);
                DS::CryptCalcX(xbuffer, nbuffer[0], kbuffer[0], CRYPT_BASE_AUTH);
                printf("Server.Auth.N \"%s\"\n", DS::Base64Encode(nbuffer[0], 64).c_str());
                printf("Server.Auth.X \"%s\"\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, nbuffer[1], kbuffer[1], CRYPT_BASE_GAME);
                printf("Server.Game.N \"%s\"\n", DS::Base64Encode(nbuffer[1], 64).c_str());
                printf("Server.Game.X \"%s\"\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, nbuffer[2], kbuffer[2], CRYPT_BASE_GATE);
                printf("Server.Gate.N \"%s\"\n", DS::Base64Encode(nbuffer[2], 64).c_str());
                printf("Server.Gate.X \"%s\"\n", DS::Base64Encode(xbuffer, 64).c_str());
                fputs("--------------------\n", stdout);
            } else if (args[1] == "show") {
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyAuth_N),
                               DS::Settings::CryptKey(DS::e_KeyAuth_K), CRYPT_BASE_AUTH);
                printf("Server.Auth.N \"%s\"\n", DS::Base64Encode(DS::Settings::CryptKey(DS::e_KeyAuth_N), 64).c_str());
                printf("Server.Auth.X \"%s\"\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGame_N),
                               DS::Settings::CryptKey(DS::e_KeyGame_K), CRYPT_BASE_GAME);
                printf("Server.Game.N \"%s\"\n", DS::Base64Encode(DS::Settings::CryptKey(DS::e_KeyGame_N), 64).c_str());
                printf("Server.Game.X \"%s\"\n", DS::Base64Encode(xbuffer, 64).c_str());
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGate_N),
                               DS::Settings::CryptKey(DS::e_KeyGate_K), CRYPT_BASE_GATE);
                printf("Server.Gate.N \"%s\"\n", DS::Base64Encode(DS::Settings::CryptKey(DS::e_KeyGate_N), 64).c_str());
                printf("Server.Gate.X \"%s\"\n", DS::Base64Encode(xbuffer, 64).c_str());
            } else {
                fputs("Error: keygen parameter should be 'new' or 'show'\n", stderr);
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
                fputs("Error: must specify on or off\n", stderr);
#else
            fputs("Error: COMM debugging is only enabled in debug builds\n", stderr);
#endif
        } else if (args[0] == "addacct") {
            if (args.size() != 3)
                fputs("Usage: addacct <user> <password>\n", stdout);
            DS::AuthServer_AddAcct(args[1], args[2]);
        } else if (args[0] == "welcome") {
            DS::Settings::SetWelcomeMsg(cmdbuf + strlen("welcome "));
        } else if (args[0] == "help") {
            fputs("DirtSand v1.0 Console supported commands:\n"
                  "    addacct <user> <password>\n"
                  "    clients\n"
                  "    commdebug <on|off>\n"
                  "    help\n"
                  "    keygen <new|show>\n"
                  "    quit\n"
                  "    restart <auth|lobby|status> [...]\n"
                  "    welcome <message>\n",
                  stdout);
        } else {
            fprintf(stderr, "Error: Unrecognized command: %s\n", args[0].c_str());
        }
    }

    DS::StopStatusHTTP();
    DS::StopLobby();
    DS::GateKeeper_Shutdown();
    DS::GameServer_Shutdown();
    DS::AuthServer_Shutdown();
    DS::FileServer_Shutdown();
    return 0;
}
