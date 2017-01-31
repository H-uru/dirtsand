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
#include "errors.h"
#include "settings.h"
#include <string_theory/codecs>
#include <string_theory/stdio>
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
        "addacct", "addallplayers", "clients", "commdebug", "globalsdl", "help", "keygen",
        "modacct", "quit", "restart", "restrict", "welcome",
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

static void sigh_crash(int signum)
{
    const char *name =
        (signum == SIGSEGV) ? "Segfault" :
        (signum == SIGABRT) ? "Abort" :
        (signum == SIGFPE)  ? "Divide by Zero" :
        (signum == SIGBUS)  ? "Bus Error" :
        "Unknown";

    print_trace(name);
    exit(4);
}

static void exception_filter()
{
    print_trace("Unhandled exception");
    exit(4);
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

    // Preset some arguments
    const char* settings = 0;
    bool restrictLogins = false;

    // Poor man command parser
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcasecmp(arg, "--restrict-logins") == 0)
            restrictLogins = true;
        else
            settings = arg;
    }

    if (settings) {
        if (!DS::Settings::LoadFrom(settings))
            return 1;
    } else {
        DS::Settings::UseDefaults();
        fputs("Warning: No config file specified. Using defaults...\n", stderr);
    }

    // Show a stackdump in case we crash
    std::set_terminate(&exception_filter);
    signal(SIGTERM, &sigh_term);
    signal(SIGSEGV, &sigh_crash);
    signal(SIGABRT, &sigh_crash);
    signal(SIGBUS, &sigh_crash);
    signal(SIGFPE, &sigh_crash);

    // Ignore sigpipe and force send() to return EPIPE
    signal(SIGPIPE, SIG_IGN);

    SDL::DescriptorDb::LoadDescriptors(DS::Settings::SdlPath());
    DS::FileServer_Init();
    DS::AuthServer_Init(restrictLogins);
    DS::GameServer_Init();
    DS::GateKeeper_Init();
    DS::StartLobby();
    if (DS::Settings::StatusEnabled())
        DS::StartStatusHTTP();

    char rl_prompt[32];
    snprintf(rl_prompt, 32, "ds-%u> ", DS::Settings::BuildId());

    char* cmdbuf = 0;
    rl_attempted_completion_function = &console_completer;
    for ( ;; ) {
        cmdbuf = readline(rl_prompt);
        if (!cmdbuf) {
            // Get us out of the prompt
            fputc('\n', stdout);
            break;
        }

        ST::string cmdline = ST::string(cmdbuf).before_first('#').trim();
        std::vector<ST::string> args = cmdline.tokenize();
        if (args.size() == 0) {
            free(cmdbuf);
            continue;
        }
        add_history(cmdbuf);
        ST::string arg_str;
        if (cmdline.size() > args.front().size() + 1)
            arg_str = ST::string(cmdbuf + args.front().size() + 1);
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
                ST::printf("Key.Auth.N = {}\n", ST::base64_encode(nbuffer[0], 64));
                ST::printf("Key.Auth.K = {}\n", ST::base64_encode(kbuffer[0], 64));
                ST::printf("Key.Game.N = {}\n", ST::base64_encode(nbuffer[1], 64));
                ST::printf("Key.Game.K = {}\n", ST::base64_encode(kbuffer[1], 64));
                ST::printf("Key.Gate.N = {}\n", ST::base64_encode(nbuffer[2], 64));
                ST::printf("Key.Gate.K = {}\n", ST::base64_encode(kbuffer[2], 64));

                fputs("--------------------\n", stdout);
                fputs("Client keys: (server.ini)\n", stdout);
                DS::CryptCalcX(xbuffer, nbuffer[0], kbuffer[0], CRYPT_BASE_AUTH);
                ST::printf("Server.Auth.N \"{}\"\n", ST::base64_encode(nbuffer[0], 64));
                ST::printf("Server.Auth.X \"{}\"\n", ST::base64_encode(xbuffer, 64));
                DS::CryptCalcX(xbuffer, nbuffer[1], kbuffer[1], CRYPT_BASE_GAME);
                ST::printf("Server.Game.N \"{}\"\n", ST::base64_encode(nbuffer[1], 64));
                ST::printf("Server.Game.X \"{}\"\n", ST::base64_encode(xbuffer, 64));
                DS::CryptCalcX(xbuffer, nbuffer[2], kbuffer[2], CRYPT_BASE_GATE);
                ST::printf("Server.Gate.N \"{}\"\n", ST::base64_encode(nbuffer[2], 64));
                ST::printf("Server.Gate.X \"{}\"\n", ST::base64_encode(xbuffer, 64));
                fputs("--------------------\n", stdout);
            } else if (args[1] == "show") {
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyAuth_N),
                               DS::Settings::CryptKey(DS::e_KeyAuth_K), CRYPT_BASE_AUTH);
                ST::printf("Server.Auth.N \"%s\"\n", ST::base64_encode(DS::Settings::CryptKey(DS::e_KeyAuth_N), 64));
                ST::printf("Server.Auth.X \"%s\"\n", ST::base64_encode(xbuffer, 64));
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGame_N),
                               DS::Settings::CryptKey(DS::e_KeyGame_K), CRYPT_BASE_GAME);
                ST::printf("Server.Game.N \"%s\"\n", ST::base64_encode(DS::Settings::CryptKey(DS::e_KeyGame_N), 64));
                ST::printf("Server.Game.X \"%s\"\n", ST::base64_encode(xbuffer, 64));
                DS::CryptCalcX(xbuffer, DS::Settings::CryptKey(DS::e_KeyGate_N),
                               DS::Settings::CryptKey(DS::e_KeyGate_K), CRYPT_BASE_GATE);
                ST::printf("Server.Gate.N \"%s\"\n", ST::base64_encode(DS::Settings::CryptKey(DS::e_KeyGate_N), 64));
                ST::printf("Server.Gate.X \"%s\"\n", ST::base64_encode(xbuffer, 64));
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
            if (args.size() != 3) {
                fputs("Usage: addacct <user> <password>\n", stdout);
                continue;
            }
            if (!DS::AuthServer_AddAcct(args[1], args[2]))
                fputs("Account was not added.\n", stderr);
        } else if (args[0] == "modacct") {
            if (args.size() < 2) {
                fputs("Usage: modacct <user> [flag]\n", stdout);
                continue;
            }
            uint32_t flags = 0;
            for (size_t i = 2; i < args.size(); ++i) {
                if (args[i] == "admin")
                    flags |= DS::e_AcctAdmin;
                else if (args[i] == "banned")
                    flags |= DS::e_AcctBanned;
                else if (args[i] == "beta")
                    flags |= DS::e_AcctBetaTester;
                else
                    ST::printf(stderr, "Warning: Unrecognized account flag \"{}\"\n", args[i]);
            }
            flags = DS::AuthServer_AcctFlags(args[1], flags);
            if (flags == static_cast<uint32_t>(-1)) {
                ST::printf(stderr, "Error: Failed to set account flags for {}\n", args[1]);
                continue;
            }
            ST::printf("{}:", args[1]);
            if (flags & DS::e_AcctAdmin)
                fputs(" [admin]", stdout);
            if (flags & DS::e_AcctBanned)
                fputs(" [banned]", stdout);
            if (flags & DS::e_AcctBetaTester)
                fputs(" [beta]", stdout);
            if (!(flags & DS::e_AcctMask))
                fputs(" [none]", stdout);
            fputs("\n", stdout);
        } else if (args[0] == "restrict") {
            bool result = DS::AuthServer_RestrictLogins();
            ST::printf("Logins are {}\n", result ? "restricted" : "unrestricted");
        } else if (args[0] == "welcome") {
            if (!DS::Settings::StatusEnabled())
                fputs("Warning:  Status server disabled.  "
                      "Setting a welcome message has no effect.\n", stderr);
            DS::Settings::SetWelcomeMsg(arg_str);
        } else if (args[0] == "addallplayers") {
            if (args.size() < 2) {
                fputs("Usage: addallplayers [playerId]\n", stdout);
                continue;
            }
            if (!DS::AuthServer_AddAllPlayersFolder(args[1].to_uint()))
                ST::printf(stderr, "Couldn't add the AllPlayers folder to {}", args[1].to_uint());
        } else if (args[0] == "globalsdl") {
            if (args.size() < 3) {
                fputs("Usage: globalsdl <ageName> <variable> <value>\n", stdout);
                continue;
            }
            ST::string value;
            if (args.size() > 3)
                value = args[3];
            if (!DS::AuthServer_ChangeGlobalSDL(args[1], args[2], value))
                ST::printf(stderr, "Error: Failed to change variable '{}'\n", args[2]);
        } else if (args[0] == "help") {
            fputs("DirtSand v1.0 Console supported commands:\n"
                  "    addacct <user> <password>\n"
                  "    addallplayers <playerId>\n"
                  "    clients\n"
                  "    commdebug <on|off>\n"
                  "    globalsdl <ageName> <variable> <value>\n"
                  "    help\n"
                  "    keygen <new|show>\n"
                  "    modacct <user> [flag]\n"
                  "    quit\n"
                  "    restart <auth|lobby|status> [...]\n"
                  "    restrict\n"
                  "    welcome <message>\n",
                  stdout);
        } else {
            ST::printf(stderr, "Error: Unrecognized command: {}\n", args[0]);
        }
    }

    if (DS::Settings::StatusEnabled())
        DS::StopStatusHTTP();
    DS::StopLobby();
    DS::GateKeeper_Shutdown();
    DS::GameServer_Shutdown();
    DS::AuthServer_Shutdown();
    DS::FileServer_Shutdown();
    return 0;
}
