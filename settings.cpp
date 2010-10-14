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

#include "settings.h"
#include "errors.h"
#include "encodings.h"
#include <vector>
#include <cstdio>

static struct
{
    /* Encryption */
    uint8_t m_cryptKeys[DS::e_KeyMaxTypes][64];
    uint32_t m_wdysKey[4];

    /* Servers */
    // TODO: Allow multiple servers for load balancing
    DS::StringBuffer<chr16_t> m_gameServ;
    DS::StringBuffer<chr16_t> m_fileServ;

    /* Database */
    DS::String m_dbHostname, m_dbPort, m_dbUsername, m_dbPassword, m_dbDbase;
} s_settings;

#define DS_LOADBLOB(outbuffer, fixedsize, input) \
    { \
        Blob* data = Base64Decode(input); \
        DS_PASSERT(data->size() == fixedsize); \
        memcpy(outbuffer, data->buffer(), fixedsize); \
        data->unref(); \
    }

#define BUF_TO_UINT(bufptr) \
    ((bufptr)[0] << 24) | ((bufptr)[1] << 16) | ((bufptr)[2] << 8) | (bufptr)[3]

bool DS::Settings::LoadFrom(const char* filename)
{
    FILE* cfgfile = fopen(filename, "r");
    if (!cfgfile) {
        fprintf(stderr, "Cannot open %s for reading\n", filename);
        return false;
    }

    try {
        char buffer[4096];
        while (fgets(buffer, 4096, cfgfile)) {
            String line = String(buffer).strip('#');
            if (line.isEmpty())
                continue;
            std::vector<String> params = line.split('=', 1);
            if (params.size() != 2) {
                fprintf(stderr, "Warning: Invalid config line: %s\n", line.c_str());
                continue;
            }

            // Clean any whitespace around the '='
            params[0] = params[0].strip();
            params[1] = params[1].strip();

            if (params[0] == "Key.Auth.N") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyAuth_N], 64, params[1]);
            } else if (params[0] == "Key.Auth.K") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyAuth_K], 64, params[1]);
            } else if (params[0] == "Key.Game.N") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGame_N], 64, params[1]);
            } else if (params[0] == "Key.Game.K") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGame_K], 64, params[1]);
            } else if (params[0] == "Key.Gate.N") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGate_N], 64, params[1]);
            } else if (params[0] == "Key.Gate.K") {
                DS_LOADBLOB(s_settings.m_cryptKeys[e_KeyGate_K], 64, params[1]);
            } else if (params[0] == "Key.WDYS") {
                Blob* data = HexDecode(params[1]);
                DS_PASSERT(data->size() == 16);
                s_settings.m_wdysKey[0] = BUF_TO_UINT(data->buffer()     );
                s_settings.m_wdysKey[1] = BUF_TO_UINT(data->buffer() +  4);
                s_settings.m_wdysKey[2] = BUF_TO_UINT(data->buffer() +  8);
                s_settings.m_wdysKey[3] = BUF_TO_UINT(data->buffer() + 12);
                data->unref();
            } else if (params[0] == "File.Host") {
                s_settings.m_fileServ = params[1].toUtf16();
            } else if (params[0] == "Game.Host") {
                s_settings.m_gameServ = params[1].toUtf16();
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
            } else {
                fprintf(stderr, "Warning: Unrecognized config parameter: %s\n", params[0].c_str());
            }
        }
    } catch (DS::AssertException ex) {
        fprintf(stderr, "[Lobby] Assertion failed at %s:%ld:  %s\n",
                ex.m_file, ex.m_line, ex.m_cond);
        return false;
    }
    fclose(cfgfile);
    return true;
}

const uint8_t* DS::Settings::CryptKey(DS::KeyType key)
{
    DS_DASSERT(static_cast<int>(key) >= 0 && static_cast<int>(key) < e_KeyMaxTypes);
    return s_settings.m_cryptKeys[key];
}

const uint32_t* DS::Settings::WdysKey()
{
    return s_settings.m_wdysKey;
}

DS::StringBuffer<chr16_t> DS::Settings::GameServerAddress()
{
    return s_settings.m_gameServ;
}

DS::StringBuffer<chr16_t> DS::Settings::FileServerAddress()
{
    return s_settings.m_fileServ;
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
