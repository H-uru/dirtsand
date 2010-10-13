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
#include <cstdio>
#include <ctype.h>

int main(int argc, char* argv[])
{
    DS::StartLobby();

    char cmdbuf[4096];
    while (fgets(cmdbuf, 4096, stdin)) {
        // Strip whitespace and comments
        char* cmdp = cmdbuf;
        while (isspace(*cmdp))
            ++cmdp;
        char* scanp = cmdp;
        while (*scanp) {
            if (*scanp == '#')
                *scanp = 0;
            else
                ++scanp;
        }
        scanp = cmdp + strlen(cmdp);
        while (scanp > cmdp && isspace(*(scanp-1)))
            --scanp;
        *scanp = 0;

        DS::String cmd = cmdp;
        if (cmd.isEmpty()) {
            /* Blank line */
            continue;
        }

        std::vector<DS::String> args = cmd.split();
        DS_DASSERT(args.size() > 0);
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
        } else if (args[0] == "help") {
            printf("DirtSand v1.0 Console supported commands:\n"
                   "    help\n"
                   "    quit\n"
                   "    restart <auth|gate|lobby>\n"
                  );
        } else {
            char* first = strtok(cmdp, " ");
            fprintf(stderr, "Error: Unrecognized command: %s\n", first);
        }
    }

    DS::StopLobby();
    return 0;
}