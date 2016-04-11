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

#ifndef _DS_FILEMANIFEST_H
#define _DS_FILEMANIFEST_H

#include "config.h"
#include "streams.h"
#include <list>

namespace DS
{
    struct FileInfo
    {
        ST::string m_filename, m_downloadName;
        char16_t m_fileHash[32], m_downloadHash[32];
        uint32_t m_fileSize, m_downloadSize;
        uint32_t m_flags;
    };

    class FileManifest
    {
    public:
        FileManifest() { }
        ~FileManifest();

        NetResultCode loadManifest(const char* filename);
        uint32_t encodeToStream(DS::Stream* stream) const;

        size_t fileCount() const { return m_files.size(); }

    private:
        std::list<FileInfo*> m_files;
    };
}

#endif
