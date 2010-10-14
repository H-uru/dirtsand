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

namespace DS
{
    enum KeyType
    {
        e_KeyGate_N, e_KeyGate_K, e_KeyAuth_N, e_KeyAuth_K, e_KeyGame_N,
        e_KeyGame_K, e_KeyMaxTypes
    };

    class Settings
    {
    public:
        static const uint8_t* CryptKey(KeyType key);
        static const uint32_t* WdysKey();

        // Optimized for throwing onto the network
        static DS::StringBuffer<chr16_t> GameServerAddress();
        static DS::StringBuffer<chr16_t> FileServerAddress();

        static const char* DbHostname();
        static const char* DbUsername();
        static const char* DbPassword();
        static const char* DbDbaseName();

        static bool LoadFrom(const char* filename);

    private:
        Settings() { }
        Settings(const Settings& settings) { }
        ~Settings() { }
    };
}

#endif
