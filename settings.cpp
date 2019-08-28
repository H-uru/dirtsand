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

#include "settings.h"
#include "errors.h"
#include "streams.h"

#include <string_theory/stdio>
#include <vector>
#include <cstdio>
#include <memory>

/* Constants configured via CMake */
uint32_t DS::Settings::BranchId()
{
    return PRODUCT_BRANCH_ID;
}

uint32_t DS::Settings::BuildId()
{
    return PRODUCT_BUILD_ID;
}

uint32_t DS::Settings::BuildType()
{
    return PRODUCT_BUILD_TYPE;
}

const char* DS::Settings::ProductUuid()
{
    return PRODUCT_UUID;
}

const char* DS::Settings::HoodUserName()
{
    return HOOD_USER_NAME;
}

const char* DS::Settings::HoodInstanceName()
{
    return HOOD_INST_NAME;
}

uint32_t DS::Settings::HoodPopThreshold()
{
    return HOOD_POP_THRESHOLD;
}


static struct
{
    /* Encryption */
    uint8_t m_cryptKeys[DS::e_KeyMaxTypes][64];
    uint32_t m_droidKey[4];

    /* Servers */
    // TODO: Allow multiple servers for load balancing
    ST::utf16_buffer m_fileServ;
    ST::utf16_buffer m_authServ;
    ST::string m_gameServ;

    /* Host configuration */
    ST::string m_lobbyAddr, m_lobbyPort;
    ST::string m_statusAddr, m_statusPort;

    /* Data locations */
    ST::string m_fileRoot, m_authRoot;
    ST::string m_sdlPath, m_agePath;
    ST::string m_settingsPath;

    /* Database */
    ST::string m_dbHostname, m_dbPort, m_dbUsername, m_dbPassword, m_dbDbase;

    /* Misc */
    bool m_statusEnabled;
    ST::string m_welcome;
} s_settings;

#define DS_LOADBLOB(outbuffer, fixedsize, params) \
    { \
        Blob data = Base64Decode(params[1]); \
        if (data.size() != fixedsize) { \
            ST::printf(stderr, "Invalid base64 blob size for {}: " \
                               "Expected {} bytes, got {} bytes\n", \
                       params[0], fixedsize, data.size()); \
            return false; \
        } \
        memcpy(outbuffer, data.buffer(), fixedsize); \
    }

#define BUF_TO_UINT(bufptr) \
    ((bufptr)[0] << 24) | ((bufptr)[1] << 16) | ((bufptr)[2] << 8) | (bufptr)[3]

bool DS::Settings::LoadFrom(const ST::string& filename)
{
    UseDefaults();

    s_settings.m_settingsPath = filename;
    ssize_t slash = s_settings.m_settingsPath.find_last('/');
    if (slash >= 0)
        s_settings.m_settingsPath = s_settings.m_settingsPath.left(slash);
    else
        s_settings.m_settingsPath = ".";

    std::unique_ptr<FILE, decltype(&fclose)> cfgfile(fopen(filename.c_str(), "r"), &fclose);
    if (!cfgfile) {
        ST::printf(stderr, "Cannot open {} for reading\n", filename);
        return false;
    }

    {
        char buffer[4096];
        while (fgets(buffer, 4096, cfgfile.get())) {
            ST::string line = ST::string(buffer).before_first('#').trim();
            if (line.empty())
                continue;
            std::vector<ST::string> params = line.split('=', 1);
            if (params.size() != 2) {
                ST::printf(stderr, "Warning: Invalid config line: {}\n", line);
                continue;
            }

            // Clean any whitespace around the '='
            params[0] = params[0].trim();
            params[1] = params[1].trim();

            if (params[0] == "Key.Auth.N") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyAuth_N], 64, params);
            } else if (params[0] == "Key.Auth.K") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyAuth_K], 64, params);
            } else if (params[0] == "Key.Game.N") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGame_N], 64, params);
            } else if (params[0] == "Key.Game.K") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGame_K], 64, params);
            } else if (params[0] == "Key.Gate.N") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGate_N], 64, params);
            } else if (params[0] == "Key.Gate.K") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGate_K], 64, params);
            } else if (params[0] == "Key.Droid") {
                Blob data = HexDecode(params[1]);
                if (data.size() != 16) {
                    fprintf(stderr, "Invalid key size for Key.Droid: "
                                    "Expected 16 bytes, got %zu bytes\n",
                            data.size());
                    return false;
                }
                s_settings.m_droidKey[0] = BUF_TO_UINT(data.buffer()     );
                s_settings.m_droidKey[1] = BUF_TO_UINT(data.buffer() +  4);
                s_settings.m_droidKey[2] = BUF_TO_UINT(data.buffer() +  8);
                s_settings.m_droidKey[3] = BUF_TO_UINT(data.buffer() + 12);
            } else if (params[0] == "File.Host") {
                s_settings.m_fileServ = params[1].to_utf16();
            } else if (params[0] == "Auth.Host") {
                s_settings.m_authServ = params[1].to_utf16();
            } else if (params[0] == "Game.Host") {
                s_settings.m_gameServ = params[1];
            } else if (params[0] == "Lobby.Addr") {
                s_settings.m_lobbyAddr = params[1];
            } else if (params[0] == "Lobby.Port") {
                s_settings.m_lobbyPort = params[1];
            } else if (params[0] == "Status.Addr") {
                s_settings.m_statusAddr = params[1];
            } else if (params[0] == "Status.Port") {
                s_settings.m_statusPort = params[1];
            } else if (params[0] == "Status.Enabled") {
                s_settings.m_statusEnabled = params[1].to_bool();
            } else if (params[0] == "File.Root") {
                s_settings.m_fileRoot = params[1];
                if (s_settings.m_fileRoot.right(1) != "/")
                    s_settings.m_fileRoot += "/";
            } else if (params[0] == "Auth.Root") {
                s_settings.m_authRoot = params[1];
                if (s_settings.m_authRoot.right(1) != "/")
                    s_settings.m_authRoot += "/";
            } else if (params[0] == "Sdl.Path") {
                s_settings.m_sdlPath = params[1];
            } else if (params[0] == "Age.Path") {
                s_settings.m_agePath = params[1];
            } else if (params[0] == "Db.Host") {
                s_settings.m_dbHostname = params[1];
            } else if (params[0] == "Db.Port") {
                s_settings.m_dbPort = params[1];
            } else if (params[0] == "Db.Username") {
                s_settings.m_dbUsername = params[1];
            } else if (params[0] == "Db.Password") {
                s_settings.m_dbPassword = params[1];
            } else if (params[0] == "Db.Database") {
                s_settings.m_dbDbase = params[1];
            } else if (params[0] == "Welcome.Msg") {
                s_settings.m_welcome = params[1];
            } else {
                ST::printf(stderr, "Warning: Unknown setting '{}' ignored\n", params[0]);
            }
        }
    }
    return true;
}

void DS::Settings::UseDefaults()
{
    memset(s_settings.m_cryptKeys, 0, sizeof(s_settings.m_cryptKeys));
    memset(s_settings.m_droidKey, 0, sizeof(s_settings.m_droidKey));

    s_settings.m_authServ = ST_LITERAL("localhost").to_utf16();
    s_settings.m_fileServ = ST_LITERAL("localhost").to_utf16();
    s_settings.m_gameServ = ST_LITERAL("localhost");
    s_settings.m_lobbyPort = ST_LITERAL("14617");
    s_settings.m_statusPort = ST_LITERAL("8080");
    s_settings.m_statusEnabled = true;

    s_settings.m_fileRoot = ST_LITERAL("./data");
    s_settings.m_authRoot = ST_LITERAL("./authdata");
    s_settings.m_sdlPath = ST_LITERAL("./SDL");
    s_settings.m_agePath = ST_LITERAL("./ages");

    s_settings.m_dbHostname = ST_LITERAL("localhost");
    s_settings.m_dbPort = ST_LITERAL("5432");
    s_settings.m_dbUsername = ST_LITERAL("dirtsand");
    s_settings.m_dbPassword = ST::null;
    s_settings.m_dbDbase = ST_LITERAL("dirtsand");
}

const uint8_t* DS::Settings::CryptKey(DS::KeyType key)
{
    DS_ASSERT(static_cast<int>(key) >= 0 && static_cast<int>(key) < e_KeyMaxTypes);
    return s_settings.m_cryptKeys[key];
}

const uint32_t* DS::Settings::DroidKey()
{
    return s_settings.m_droidKey;
}

ST::utf16_buffer DS::Settings::FileServerAddress()
{
    return s_settings.m_fileServ;
}

ST::utf16_buffer DS::Settings::AuthServerAddress()
{
    return s_settings.m_authServ;
}

ST::string DS::Settings::GameServerAddress()
{
    return s_settings.m_gameServ;
}

const char* DS::Settings::LobbyAddress()
{
    return s_settings.m_lobbyAddr.empty()
                ? "127.0.0.1"   /* Not useful for external connections */
                : s_settings.m_lobbyAddr.c_str();
}

const char* DS::Settings::LobbyPort()
{
    return s_settings.m_lobbyPort.c_str();
}

bool DS::Settings::StatusEnabled()
{
    return s_settings.m_statusEnabled;
}

const char* DS::Settings::StatusAddress()
{
    return s_settings.m_statusAddr.empty()
                ? "127.0.0.1"   /* Not useful for external connections */
                : s_settings.m_statusAddr.c_str();
}

const char* DS::Settings::StatusPort()
{
    return s_settings.m_statusPort.c_str();
}

ST::string DS::Settings::FileRoot()
{
    return s_settings.m_fileRoot;
}

ST::string DS::Settings::AuthRoot()
{
    return s_settings.m_authRoot;
}

const char* DS::Settings::SdlPath()
{
    return s_settings.m_sdlPath.c_str();
}

const char* DS::Settings::AgePath()
{
    return s_settings.m_agePath.c_str();
}

ST::string DS::Settings::SettingsPath()
{
    return s_settings.m_settingsPath;
}

const char* DS::Settings::DbHostname()
{
    return s_settings.m_dbHostname.c_str();
}

const char* DS::Settings::DbPort()
{
    return s_settings.m_dbPort.c_str();
}

const char* DS::Settings::DbUsername()
{
    return s_settings.m_dbUsername.c_str();
}

const char* DS::Settings::DbPassword()
{
    return s_settings.m_dbPassword.c_str();
}

const char* DS::Settings::DbDbaseName()
{
    return s_settings.m_dbDbase.c_str();
}

ST::string DS::Settings::WelcomeMsg()
{
    return s_settings.m_welcome;
}

void DS::Settings::SetWelcomeMsg(const ST::string& welcome)
{
    s_settings.m_welcome = welcome;
}
