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
#include <memory>

/* Cyan makes their SHA hashes 5 LE dwords instead of 20 bytes, making our
 * lives more difficult and annoying.
 */
#define SWAP_BYTES(x) __builtin_bswap32((x))
#define SWAP_BYTES64(x) __builtin_bswap64((x))

DS::ShaHash::ShaHash(const char* struuid)
{
    char buffer[9];
    buffer[8] = 0;
    for (size_t i=0; i<5; ++i) {
        memcpy(buffer, struuid + (i * 8), 8);
        m_data[i] = strtoul(buffer, nullptr, 16);
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

static DS::ShaHash _ds_internal_sha0(const void *data, size_t size);

DS::ShaHash DS::ShaHash::Sha0(const void* data, size_t size)
{
    const EVP_MD* sha0_md = EVP_get_digestbyname("sha");
    if (!sha0_md) {
        // Use our own implementation only if OpenSSL doesn't support it
        return _ds_internal_sha0(data, size);
    }

    ShaHash result;
    unsigned int out_len = EVP_MD_size(sha0_md);
    DS_ASSERT(out_len == sizeof(result.m_data));

    EVP_MD_CTX* sha_ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(sha_ctx, sha0_md, nullptr);
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
    EVP_DigestInit_ex(sha1_ctx, sha1_md, nullptr);
    EVP_DigestUpdate(sha1_ctx, data, size);
    EVP_DigestFinal_ex(sha1_ctx, reinterpret_cast<unsigned char *>(result.m_data), &out_len);
    EVP_MD_CTX_destroy(sha1_ctx);

    return result;
}

constexpr uint32_t rol32(uint32_t value, unsigned int n)
{
    // GCC and Clang will optimize this to a single rol instruction on x86
    return (value << n) | (value >> (32 - n));
}

static DS::ShaHash _ds_internal_sha0(const void* data, size_t size)
{
    uint32_t hash[5] = {
        0x67452301,
        0xefcdab89,
        0x98badcfe,
        0x10325476,
        0xc3d2e1f0
    };

    uint8_t stack_buffer[256];
    std::unique_ptr<uint8_t[]> heap_buffer;
    uint8_t *bufp;

    // Preprocess the message to pad it to a multiple of 512 bits,
    // with the (Big Endian) size in bits tacked onto the end.
    uint64_t buf_size = size + 1 + sizeof(uint64_t);
    if ((buf_size % 64) != 0)
        buf_size += 64 - (buf_size % 64);
    if (buf_size > sizeof(stack_buffer)) {
        heap_buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
        bufp = heap_buffer.get();
    } else {
        bufp = stack_buffer;
    }

    memcpy(bufp, data, size);
    memset(bufp + size, 0, buf_size - size);
    bufp[size] = 0x80;  // Append '1' bit to the end of the message
    uint64_t msg_size_bits = SWAP_BYTES64(static_cast<uint64_t>(size) * 8);
    memcpy(bufp + buf_size - sizeof(msg_size_bits),
           &msg_size_bits, sizeof(msg_size_bits));

    uint8_t* end = bufp + buf_size;
    uint32_t work[80];
    while (bufp < end) {
        memcpy(work, bufp, 64);
        bufp += 64;

        for (size_t i = 0; i < 16; ++i)
            work[i] = SWAP_BYTES(work[i]);

        for (size_t i = 16; i < 80; ++i) {
            // SHA-1 difference: no rol32(work[i], 1)
            work[i] = work[i-3] ^ work[i-8] ^ work[i-14] ^ work[i-16];
        }

        uint32_t hv[5];
        memcpy(hv, hash, sizeof(hv));

        // Main SHA loop
        for (size_t i = 0; i < 20; ++i) {
            constexpr uint32_t K = 0x5a827999;
            const uint32_t f = (hv[1] & hv[2]) | (~hv[1] & hv[3]);
            const uint32_t temp = rol32(hv[0], 5) + f + hv[4] + K + work[i];
            hv[4] = hv[3];
            hv[3] = hv[2];
            hv[2] = rol32(hv[1], 30);
            hv[1] = hv[0];
            hv[0] = temp;
        }
        for (size_t i = 20; i < 40; ++i) {
            constexpr uint32_t K = 0x6ed9eba1;
            const uint32_t f = (hv[1] ^ hv[2] ^ hv[3]);
            const uint32_t temp = rol32(hv[0], 5) + f + hv[4] + K + work[i];
            hv[4] = hv[3];
            hv[3] = hv[2];
            hv[2] = rol32(hv[1], 30);
            hv[1] = hv[0];
            hv[0] = temp;
        }
        for (size_t i = 40; i < 60; ++i) {
            constexpr uint32_t K = 0x8f1bbcdc;
            const uint32_t f = (hv[1] & hv[2]) | (hv[1] & hv[3]) | (hv[2] & hv[3]);
            const uint32_t temp = rol32(hv[0], 5) + f + hv[4] + K + work[i];
            hv[4] = hv[3];
            hv[3] = hv[2];
            hv[2] = rol32(hv[1], 30);
            hv[1] = hv[0];
            hv[0] = temp;
        }
        for (size_t i = 60; i < 80; ++i) {
            constexpr uint32_t K = 0xca62c1d6;
            const uint32_t f = (hv[1] ^ hv[2] ^ hv[3]);
            const uint32_t temp = rol32(hv[0], 5) + f + hv[4] + K + work[i];
            hv[4] = hv[3];
            hv[3] = hv[2];
            hv[2] = rol32(hv[1], 30);
            hv[1] = hv[0];
            hv[0] = temp;
        }

        hash[0] += hv[0];
        hash[1] += hv[1];
        hash[2] += hv[2];
        hash[3] += hv[3];
        hash[4] += hv[4];
    }

    // Bring the output back to host endian
    for (size_t i = 0; i < 5; ++i)
        hash[i] = SWAP_BYTES(hash[i]);

    return DS::ShaHash(reinterpret_cast<const uint8_t*>(hash));
}

#ifdef DS_BUILD_SHA0_TESTS
#define EXPECT_SHA(sha, expr)                                           \
{                                                                       \
    const DS::ShaHash hash = (expr);                                    \
    if (hash.toString() != sha) {                                       \
        fputs("Incorrect SHA hash for " #expr ":\n", stderr);           \
        fputs("  Expected: " sha "\n", stderr);                         \
        ST::printf(stderr, "  Actual:   {}\n", hash.toString());        \
        ++test_fails;                                                   \
    }                                                                   \
}

int main()
{
    int test_fails = 0;

    OpenSSL_add_all_digests();

    // Sanity check EXPECT_SHA with known-good SHA-1 implementation
    EXPECT_SHA("da39a3ee5e6b4b0d3255bfef95601890afd80709",
               DS::ShaHash::Sha1("", 0));
    EXPECT_SHA("a9993e364706816aba3e25717850c26c9cd0d89d",
               DS::ShaHash::Sha1("abc", 3));

    // Now the SHA-0 tests
    EXPECT_SHA("f96cea198ad1dd5617ac084a3d92c6107708c0ef",
               DS::ShaHash::Sha0("", 0));
    EXPECT_SHA("37f297772fae4cb1ba39b6cf9cf0381180bd62f2",
               DS::ShaHash::Sha0("a", 1));
    EXPECT_SHA("0164b8a914cd2a5e74c4f7ff082c4d97f1edf880",
               DS::ShaHash::Sha0("abc", 3));
    EXPECT_SHA("d2516ee1acfa5baf33dfc1c471e438449ef134c8",
               DS::ShaHash::Sha0("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56));
    EXPECT_SHA("459f83b95db2dc87bb0f5b513a28f900ede83237",
               DS::ShaHash::Sha0("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                                 "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 112));

    if (test_fails == 0)
        fputs("All tests passed!\n", stdout);
    return test_fails;
}
#endif
