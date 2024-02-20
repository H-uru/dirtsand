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

#ifndef _DS_AUTHMANIFEST_H
#define _DS_AUTHMANIFEST_H

#include "config.h"
#include "streams.h"
#include <list>

namespace DS
{
    struct AuthFileInfo
    {
        AuthFileInfo() : m_fileSize() { }
        AuthFileInfo(ST::string filename, uint32_t fileSize)
            : m_filename(std::move(filename)), m_fileSize(fileSize)
        { }

        AuthFileInfo(const AuthFileInfo&) = delete;
        AuthFileInfo& operator=(const AuthFileInfo&) = delete;

        AuthFileInfo(AuthFileInfo&&) = default;
        AuthFileInfo& operator=(AuthFileInfo&&) = default;

        ST::string m_filename;
        uint32_t m_fileSize;
    };

    class AuthManifest
    {
    public:
        AuthManifest() { }

        NetResultCode loadManifest(const char* filename);
        uint32_t encodeToStream(DS::Stream* stream) const;

        size_t fileCount() const { return m_files.size(); }

        void addFile(ST::string filename, uint32_t fileSize)
        {
            m_files.emplace_back(std::move(filename), fileSize);
        }

    private:
        std::vector<AuthFileInfo> m_files;
    };
}

#endif
