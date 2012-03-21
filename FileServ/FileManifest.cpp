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
#include "errors.h"
#include <cstdio>

DS::FileManifest::~FileManifest()
{
    while (!m_files.empty()) {
        delete m_files.front();
        m_files.pop_front();
    }
}

DS::NetResultCode DS::FileManifest::loadManifest(const char* filename)
{
    FILE* mfs = fopen(filename, "r");
    if (!mfs)
        return e_NetFileNotFound;

    char lnbuf[4096];
    long mfsline = 0;
    while (fgets(lnbuf, 4096, mfs)) {
        ++mfsline;
        String line = String(lnbuf).strip('#');
        if (line.isEmpty())
            continue;

        std::vector<String> parts = line.split(',');
        if (parts.size() != 7) {
            fprintf(stderr, "Warning:  Ignoring invalid manifest entry on line %ld of %s:\n",
                    mfsline, filename);
            continue;
        }
        if (parts[2].length() != 32 || parts[3].length() != 32) {
            fprintf(stderr, "Warning:  Bad file hash on line %ld of %s:\n",
                    mfsline, filename);
            continue;
        }

        FileInfo* info = new FileInfo;
        info->m_filename = parts[0];
        info->m_downloadName = parts[1];
        memcpy(info->m_fileHash, parts[2].toUtf16().data(), 32 * sizeof(chr16_t));
        memcpy(info->m_downloadHash, parts[3].toUtf16().data(), 32 * sizeof(chr16_t));
        info->m_fileSize = parts[4].toUint();
        info->m_downloadSize = parts[5].toUint();
        info->m_flags = parts[6].toUint();
        m_files.push_back(info);
    }

    fclose(mfs);
    return e_NetSuccess;
}

uint32_t DS::FileManifest::encodeToStream(DS::Stream* stream)
{
    uint32_t start = stream->tell();

    for (auto it = m_files.begin(); it != m_files.end(); ++it) {
        StringBuffer<chr16_t> wstrbuf = (*it)->m_filename.toUtf16();
        stream->writeBytes(wstrbuf.data(), wstrbuf.length() * sizeof(chr16_t));
        stream->write<chr16_t>(0);

        wstrbuf = (*it)->m_downloadName.toUtf16();
        stream->writeBytes(wstrbuf.data(), wstrbuf.length() * sizeof(chr16_t));
        stream->write<chr16_t>(0);

        stream->writeBytes((*it)->m_fileHash, 32 * sizeof(chr16_t));
        stream->write<chr16_t>(0);

        stream->writeBytes((*it)->m_downloadHash, 32 * sizeof(chr16_t));
        stream->write<chr16_t>(0);

        stream->write<uint16_t>((*it)->m_fileSize >> 16);
        stream->write<uint16_t>((*it)->m_fileSize & 0xFFFF);
        stream->write<uint16_t>(0);

        stream->write<uint16_t>((*it)->m_downloadSize >> 16);
        stream->write<uint16_t>((*it)->m_downloadSize & 0xFFFF);
        stream->write<uint16_t>(0);

        stream->write<uint16_t>((*it)->m_flags >> 16);
        stream->write<uint16_t>((*it)->m_flags & 0xFFFF);
        stream->write<uint16_t>(0);
    }
    stream->write<uint16_t>(0);

    DS_DASSERT((stream->tell() - start) % sizeof(chr16_t) == 0);
    return (stream->tell() - start) / sizeof(chr16_t);
}
