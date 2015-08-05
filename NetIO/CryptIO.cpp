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

#include "CryptIO.h"
#include "errors.h"
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/rc4.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdio>
#include <mutex>
#include <memory>

#ifdef DEBUG
bool s_commdebug = false;
std::mutex s_commdebug_mutex;
#endif

static void init_rand()
{
    static bool _rand_seeded = false;
    if (!_rand_seeded) {
        struct {
            pid_t   mypid;
            timeval now;
            uint8_t buffer[2048 - sizeof(pid_t) - sizeof(timeval)];
        } _random;
        _random.mypid = getpid();
        gettimeofday(&_random.now, 0);
        FILE* urand = fopen("/dev/urandom", "rb");
        DS_PASSERT(urand != 0);
        fread(_random.buffer, 1, sizeof(_random.buffer), urand);
        fclose(urand);
        RAND_seed(&_random, sizeof(_random));
        _rand_seeded = true;
    }
}

void DS::GenPrimeKeys(uint8_t* K, uint8_t* N)
{
    BIGNUM* bn_key = BN_new();
    init_rand();

    while (BN_num_bytes(bn_key) != 64) {
        BN_generate_prime(bn_key, 512, 1, 0, 0, 0, 0);
        putc('.', stdout);
        fflush(stdout);
    }
    BN_bn2bin(bn_key, reinterpret_cast<unsigned char*>(K));
    BN_set_word(bn_key, 0);
    while (BN_num_bytes(bn_key) != 64) {
        BN_generate_prime(bn_key, 512, 1, 0, 0, 0, 0);
        putc('.', stdout);
        fflush(stdout);
    }
    BN_bn2bin(bn_key, reinterpret_cast<unsigned char*>(N));

    BN_free(bn_key);
}

void DS::CryptCalcX(uint8_t* X, const uint8_t* N, const uint8_t* K, uint32_t base)
{
    BIGNUM* bn_X = BN_new();
    BIGNUM* bn_N = BN_new();
    BIGNUM* bn_K = BN_new();
    BIGNUM* bn_G = BN_new();
    BN_CTX* ctx = BN_CTX_new();

    /* X = base ^ K % N */
    BN_bin2bn(reinterpret_cast<const unsigned char*>(N), 64, bn_N);
    BN_bin2bn(reinterpret_cast<const unsigned char*>(K), 64, bn_K);
    BN_set_word(bn_G, base);
    BN_mod_exp(bn_X, bn_G, bn_K, bn_N, ctx);
    BN_bn2bin(bn_X, reinterpret_cast<unsigned char*>(X));

    BN_free(bn_X);
    BN_free(bn_N);
    BN_free(bn_K);
    BN_free(bn_G);
    BN_CTX_free(ctx);
}

void DS::CryptEstablish(uint8_t* seed, uint8_t* key, const uint8_t* N,
                        const uint8_t* K, uint8_t* Y)
{
    BIGNUM* bn_Y = BN_new();
    BIGNUM* bn_N = BN_new();
    BIGNUM* bn_K = BN_new();
    BIGNUM* bn_seed = BN_new();
    BN_CTX* ctx = BN_CTX_new();

    /* Random 7-byte server seed */
    init_rand();
    RAND_bytes(reinterpret_cast<unsigned char*>(seed), 7);

    /* client = Y ^ K % N */
    BN_bin2bn(reinterpret_cast<const unsigned char*>(Y), 64, bn_Y);
    BN_bin2bn(reinterpret_cast<const unsigned char*>(N), 64, bn_N);
    BN_bin2bn(reinterpret_cast<const unsigned char*>(K), 64, bn_K);
    DS_PASSERT(!BN_is_zero(bn_N));
    BN_mod_exp(bn_seed, bn_Y, bn_K, bn_N, ctx);

    /* Apply server seed for establishing crypt state with client */
    uint8_t keybuf[64];
    DS_DASSERT(BN_num_bytes(bn_seed) <= 64);
    size_t outBytes = BN_bn2bin(bn_seed, reinterpret_cast<unsigned char*>(keybuf));
    BYTE_SWAP_BUFFER(keybuf, outBytes);
    for (size_t i=0; i<7; ++i)
        key[i] = keybuf[i] ^ seed[i];

    BN_free(bn_Y);
    BN_free(bn_N);
    BN_free(bn_K);
    BN_free(bn_seed);
    BN_CTX_free(ctx);
}


struct CryptState_Private
{
    RC4_KEY m_writeKey;
    RC4_KEY m_readKey;
};

DS::CryptState DS::CryptStateInit(const uint8_t* key, size_t size)
{
    CryptState_Private* state = new CryptState_Private;
    RC4_set_key(&state->m_readKey, size, key);
    RC4_set_key(&state->m_writeKey, size, key);
    return reinterpret_cast<CryptState>(state);
}

void DS::CryptStateFree(DS::CryptState state)
{
    if (!state)
        return;

    CryptState_Private* statep = reinterpret_cast<CryptState_Private*>(state);
    delete statep;
}

void DS::CryptSendBuffer(const DS::SocketHandle sock, DS::CryptState crypt,
                         const void* buffer, size_t size)
{
#ifdef DEBUG
    if (s_commdebug) {
        std::lock_guard<std::mutex> commdebugGuard(s_commdebug_mutex);
        printf("SEND TO %s", DS::SockIpAddress(sock).c_str());
        for (size_t i=0; i<size; ++i) {
            if ((i % 16) == 0)
                fputs("\n    ", stdout);
            else if ((i % 16) == 8)
                fputs("   ", stdout);
            printf("%02X ", reinterpret_cast<const uint8_t*>(buffer)[i]);
        }
        fputc('\n', stdout);
    }
#endif

    CryptState_Private* statep = reinterpret_cast<CryptState_Private*>(crypt);
    if (!statep) {
        DS::SendBuffer(sock, buffer, size);
    } else if (size > 4096) {
        std::unique_ptr<unsigned char[]> cryptbuf(new unsigned char[size]);
        RC4(&statep->m_writeKey, size, reinterpret_cast<const unsigned char*>(buffer), cryptbuf.get());
        DS::SendBuffer(sock, cryptbuf.get(), size);
    } else {
        unsigned char stack[4096];
        RC4(&statep->m_writeKey, size, reinterpret_cast<const unsigned char*>(buffer), stack);
        DS::SendBuffer(sock, stack, size);
    }
}

void DS::CryptRecvBuffer(const DS::SocketHandle sock, DS::CryptState crypt,
                         void* buffer, size_t size)
{
    CryptState_Private* statep = reinterpret_cast<CryptState_Private*>(crypt);
    if (!statep) {
        DS::RecvBuffer(sock, buffer, size);
    } else if (size > 4096) {
        std::unique_ptr<unsigned char[]> cryptbuf(new unsigned char[size]);
        DS::RecvBuffer(sock, cryptbuf.get(), size);
        RC4(&statep->m_readKey, size, cryptbuf.get(), reinterpret_cast<unsigned char*>(buffer));
    } else {
        unsigned char stack[4096];
        DS::RecvBuffer(sock, stack, size);
        RC4(&statep->m_readKey, size, stack, reinterpret_cast<unsigned char*>(buffer));
    }

#ifdef DEBUG
    if (s_commdebug) {
        std::lock_guard<std::mutex> commdebugGuard(s_commdebug_mutex);
        printf("RECV FROM %s", DS::SockIpAddress(sock).c_str());
        for (size_t i=0; i<size; ++i) {
            if ((i % 16) == 0)
                fputs("\n    ", stdout);
            else if ((i % 16) == 8)
                fputs("   ", stdout);
            printf("%02X ", reinterpret_cast<const uint8_t*>(buffer)[i]);
        }
        fputc('\n', stdout);
    }
#endif
}

DS::String DS::CryptRecvString(const SocketHandle sock, CryptState crypt)
{
    uint16_t length = CryptRecvValue<uint16_t>(sock, crypt);
    std::unique_ptr<char16_t[]> buffer(new char16_t[length]);
    CryptRecvBuffer(sock, crypt, buffer.get(), length * sizeof(char16_t));
    String result = String::FromUtf16(buffer.get(), length);
    return result;
}

DS::ShaHash DS::BuggyHashPassword(const String& username, const String& password)
{
    StringBuffer<char16_t> wuser = username.toUtf16();
    StringBuffer<char16_t> wpass = password.toUtf16();
    char16_t* buffer = new char16_t[wuser.length() + wpass.length()];
    memcpy(buffer, wpass.data(), wpass.length() * sizeof(char16_t));
    memcpy(buffer + wpass.length(), wuser.data(), wuser.length() * sizeof(char16_t));
    buffer[wpass.length() - 1] = 0;
    buffer[wpass.length() + wuser.length() - 1] = 0;
    ShaHash hash = ShaHash::Sha0(buffer, (wuser.length() + wpass.length()) * sizeof(char16_t));
    delete[] buffer;
    return hash;
}

DS::ShaHash DS::BuggyHashLogin(const ShaHash& passwordHash, uint32_t serverChallenge,
                               uint32_t clientChallenge)
{
    struct
    {
        uint32_t m_clientChallenge, m_serverChallenge;
        ShaHash m_pwhash;
    } buffer;

    buffer.m_clientChallenge = clientChallenge;
    buffer.m_serverChallenge = serverChallenge;
    buffer.m_pwhash = passwordHash;
    return ShaHash::Sha0(&buffer, sizeof(buffer));
}
