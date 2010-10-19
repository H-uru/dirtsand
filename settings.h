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

#ifndef _DS_SETTINGS_H
#define _DS_SETTINGS_H

#include "strings.h"

#define CRYPT_BASE_AUTH (41)
#define CRYPT_BASE_GAME (73)
#define CRYPT_BASE_GATE (4)

#define CLIENT_BUILD_ID   (893)
#define CLIENT_BUILD_TYPE (50)
#define CLIENT_BRANCH_ID  (1)
#define CLIENT_PRODUCT_ID "ea489821-6c35-4bd0-9dae-bb17c585e680"

#define CHUNK_SIZE (0x8000)

namespace DS
{
    enum KeyType
    {
        e_KeyGate_N, e_KeyGate_K, e_KeyAuth_N, e_KeyAuth_K, e_KeyGame_N,
        e_KeyGame_K, e_KeyMaxTypes
    };

    namespace Settings
    {
        const uint8_t* CryptKey(KeyType key);
        const uint32_t* WdysKey();

        // Optimized for throwing onto the network
        DS::StringBuffer<chr16_t> FileServerAddress();
        DS::StringBuffer<chr16_t> AuthServerAddress();
        DS::StringBuffer<chr16_t> GameServerAddress();

        DS::String FileRoot();
        DS::String AuthRoot();

        const char* DbHostname();
        const char* DbPort();
        const char* DbUsername();
        const char* DbPassword();
        const char* DbDbaseName();

        bool LoadFrom(const char* filename);
        void UseDefaults();
    }
}

extern bool s_authServerRunning;

#endif
