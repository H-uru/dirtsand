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
#include "strings.h"
#include "errors.h"
#include "settings.h"
#include <cstdio>

int main(int argc, char* argv[])
{
    if (argc == 1) {
        fprintf(stderr, "Error: No config file specified\n");
        return 1;
    }
    if (!DS::Settings::LoadFrom(argv[1]))
        return 1;

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
                    fprintf(stderr, "Error: Unrecognized service: %s\n", svc->c_str());
                }
            }
        } else if (args[0] == "keygen") {
            //
        } else if (args[0] == "help") {
            printf("DirtSand v1.0 Console supported commands:\n"
                   "    help\n"
                   "    keygen <auth|gate|game>\n"
                   "    quit\n"
                   "    restart <auth|gate|lobby>\n"
                  );
        } else {
            fprintf(stderr, "Error: Unrecognized command: %s\n", args[0].c_str());
        }
    }

    DS::StopLobby();
    return 0;
}