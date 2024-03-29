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

#include "FileManifest.h"
#include <string_theory/stdio>
#include <cstdio>

DS::NetResultCode DS::FileManifest::loadManifest(const char* filename)
{
    FILE* mfs = fopen(filename, "r");
    if (!mfs)
        return e_NetFileNotFound;

    char lnbuf[4096];
    long mfsline = 0;
    while (fgets(lnbuf, 4096, mfs)) {
        ++mfsline;
        ST::string line = ST::string(lnbuf).before_first('#').trim();
        if (line.empty())
            continue;

        std::vector<ST::string> parts = line.split(',');
        if (parts.size() != 7) {
            ST::printf(stderr, "Warning:  Ignoring invalid manifest entry on line {} of {}:\n",
                       mfsline, filename);
            continue;
        }
        if (parts[2].size() != 32 || parts[3].size() != 32) {
            ST::printf(stderr, "Warning:  Bad file hash on line {} of {}:\n",
                       mfsline, filename);
            continue;
        }

        FileInfo& info = m_files.emplace_back();
        info.m_filename = parts[0];
        info.m_downloadName = parts[1];
        memcpy(info.m_fileHash, parts[2].to_utf16().data(), 32 * sizeof(char16_t));
        memcpy(info.m_downloadHash, parts[3].to_utf16().data(), 32 * sizeof(char16_t));
        info.m_fileSize = parts[4].to_uint();
        info.m_downloadSize = parts[5].to_uint();
        info.m_flags = parts[6].to_uint();
    }

    fclose(mfs);
    return e_NetSuccess;
}

uint32_t DS::FileManifest::encodeToStream(DS::Stream* stream) const
{
    uint32_t start = stream->tell();

    for (const FileInfo& info : m_files) {
        stream->writeString(info.m_filename, DS::e_StringUTF16);
        stream->write<char16_t>(0);

        stream->writeString(info.m_downloadName, DS::e_StringUTF16);
        stream->write<char16_t>(0);

        stream->writeBytes(info.m_fileHash, sizeof(FileInfo::m_fileHash));
        stream->write<char16_t>(0);

        stream->writeBytes(info.m_downloadHash, sizeof(FileInfo::m_downloadHash));
        stream->write<char16_t>(0);

        stream->write<uint16_t>(info.m_fileSize >> 16);
        stream->write<uint16_t>(info.m_fileSize & 0xFFFF);
        stream->write<uint16_t>(0);

        stream->write<uint16_t>(info.m_downloadSize >> 16);
        stream->write<uint16_t>(info.m_downloadSize & 0xFFFF);
        stream->write<uint16_t>(0);

        stream->write<uint16_t>(info.m_flags >> 16);
        stream->write<uint16_t>(info.m_flags & 0xFFFF);
        stream->write<uint16_t>(0);
    }
    stream->write<uint16_t>(0);

    if ((stream->tell() - start) % sizeof(char16_t) != 0) {
        fputs("WARNING: Encoded manifest data was not evenly divisible by "
              "UTF-16 character size.  The recipient client may crash...\n",
              stderr);
    }
    return (stream->tell() - start) / sizeof(char16_t);
}
