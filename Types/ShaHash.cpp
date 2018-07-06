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

#include "ShaHash.h"
#include "errors.h"
#include <string_theory/format>
#include <openssl/evp.h>
#include <cstdlib>

/* Cyan makes their SHA hashes 5 LE dwords instead of 20 bytes, making our
 * lives more difficult and annoying.
 */
#define SWAP_BYTES(x) (((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) \
                   |  (((x) & 0x000000FF) << 24) | (((x) & 0x0000FF00) << 8)

DS::ShaHash::ShaHash(const char* struuid)
{
    char buffer[9];
    buffer[8] = 0;
    for (size_t i=0; i<5; ++i) {
        memcpy(buffer, struuid + (i * 8), 8);
        m_data[i] = strtoul(buffer, 0, 16);
        m_data[i] = SWAP_BYTES(m_data[i]);
    }
}

void DS::ShaHash::read(DS::Stream* stream)
{
    m_data[0] = stream->read<uint32_t>();
    m_data[1] = stream->read<uint32_t>();
    m_data[2] = stream->read<uint32_t>();
    m_data[3] = stream->read<uint32_t>();
    m_data[4] = stream->read<uint32_t>();
}

void DS::ShaHash::write(DS::Stream* stream) const
{
    stream->write<uint32_t>(m_data[0]);
    stream->write<uint32_t>(m_data[1]);
    stream->write<uint32_t>(m_data[2]);
    stream->write<uint32_t>(m_data[3]);
    stream->write<uint32_t>(m_data[4]);
}

void DS::ShaHash::swapBytes()
{
    m_data[0] = SWAP_BYTES(m_data[0]);
    m_data[1] = SWAP_BYTES(m_data[1]);
    m_data[2] = SWAP_BYTES(m_data[2]);
    m_data[3] = SWAP_BYTES(m_data[3]);
    m_data[4] = SWAP_BYTES(m_data[4]);
}

ST::string DS::ShaHash::toString() const
{
    return ST::format("{08x}{08x}{08x}{08x}{08x}",
                      SWAP_BYTES(m_data[0]), SWAP_BYTES(m_data[1]),
                      SWAP_BYTES(m_data[2]), SWAP_BYTES(m_data[3]),
                      SWAP_BYTES(m_data[4]));
}

DS::ShaHash DS::ShaHash::Sha0(const void* data, size_t size)
{
    ShaHash result;
    const EVP_MD* sha0_md = EVP_get_digestbyname("sha");
    DS_ASSERT(sha0_md);

    unsigned int out_len = EVP_MD_size(sha0_md);
    DS_ASSERT(out_len == sizeof(result.m_data));

    EVP_MD_CTX* sha_ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(sha_ctx, sha0_md, NULL);
    EVP_DigestUpdate(sha_ctx, data, size);
    EVP_DigestFinal_ex(sha_ctx, reinterpret_cast<unsigned char *>(result.m_data), &out_len);
    EVP_MD_CTX_destroy(sha_ctx);

    return result;
}

DS::ShaHash DS::ShaHash::Sha1(const void* data, size_t size)
{
    ShaHash result;
    const EVP_MD* sha1_md = EVP_get_digestbyname("sha1");
    DS_ASSERT(sha1_md);

    unsigned int out_len = EVP_MD_size(sha1_md);
    DS_ASSERT(out_len == sizeof(result.m_data));

    EVP_MD_CTX* sha1_ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(sha1_ctx, sha1_md, NULL);
    EVP_DigestUpdate(sha1_ctx, data, size);
    EVP_DigestFinal_ex(sha1_ctx, reinterpret_cast<unsigned char *>(result.m_data), &out_len);
    EVP_MD_CTX_destroy(sha1_ctx);

    return result;
}
