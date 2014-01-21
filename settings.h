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

#ifndef _DS_SETTINGS_H
#define _DS_SETTINGS_H

#include "strings.h"

#define CRYPT_BASE_AUTH (41)
#define CRYPT_BASE_GAME (73)
#define CRYPT_BASE_GATE (4)

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
        // Product ID values
        uint32_t BranchId();
        uint32_t BuildId();
        uint32_t BuildType();
        const char* ProductUuid();

        // Neighborhood Defaults
        const char* HoodUserName();
        const char* HoodInstanceName();
        uint32_t HoodPopThreshold();

        // Encryption keys
        const uint8_t* CryptKey(KeyType key);
        const uint32_t* DroidKey();

        // Optimized for throwing onto the network
        DS::StringBuffer<char16_t> FileServerAddress();
        DS::StringBuffer<char16_t> AuthServerAddress();
        DS::String GameServerAddress();

        const char* LobbyAddress();
        const char* LobbyPort();

        bool StatusEnabled();
        const char* StatusAddress();
        const char* StatusPort();

        DS::String FileRoot();
        DS::String AuthRoot();
        const char* SdlPath();
        const char* AgePath();
        DS::String SettingsPath();

        const char* DbHostname();
        const char* DbPort();
        const char* DbUsername();
        const char* DbPassword();
        const char* DbDbaseName();

        DS::String WelcomeMsg();
        void SetWelcomeMsg(const DS::String& welcome);

        bool LoadFrom(const char* filename);
        void UseDefaults();
    }
}

#endif
