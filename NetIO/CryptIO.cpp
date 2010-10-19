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

#include "CryptIO.h"
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/rc4.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdio>

#ifdef DEBUG
bool s_commdebug = false;
pthread_mutex_t s_commdebug_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void init_rand()
{
    static bool _rand_seeded = false;
    if (!_rand_seeded) {
        struct {
            pid_t mypid;
            timeval now;
        } _random;
        _random.mypid = getpid();
        gettimeofday(&_random.now, 0);
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
        printf(".");
        fflush(stdout);
    }
    BN_bn2bin(bn_key, reinterpret_cast<unsigned char*>(K));
    BN_set_word(bn_key, 0);
    while (BN_num_bytes(bn_key) != 64) {
        BN_generate_prime(bn_key, 512, 1, 0, 0, 0, 0);
        printf(".");
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

void DS::PrintClientKeys(const uint8_t* X, const uint8_t* N)
{
    /* These are put into large blocks so a single line will go to stdout
     * at a time (in case other threads need to write).  Eventually, perhaps
     * some form of I/O control/locking would be better, but for now this
     * works well enough
     */

    uint8_t swapbuf[64];
    memcpy(swapbuf, N, 64);
    BYTE_SWAP_BUFFER(swapbuf, 64);
    printf("N = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[ 0], swapbuf[ 1], swapbuf[ 2], swapbuf[ 3], swapbuf[ 4], swapbuf[ 5], swapbuf[ 6], swapbuf[ 7],
           swapbuf[ 8], swapbuf[ 9], swapbuf[10], swapbuf[11], swapbuf[12], swapbuf[13], swapbuf[14], swapbuf[15]);
    printf("    %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[16], swapbuf[17], swapbuf[18], swapbuf[19], swapbuf[20], swapbuf[21], swapbuf[22], swapbuf[23],
           swapbuf[24], swapbuf[25], swapbuf[26], swapbuf[27], swapbuf[28], swapbuf[29], swapbuf[30], swapbuf[31]);
    printf("    %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[32], swapbuf[33], swapbuf[34], swapbuf[35], swapbuf[36], swapbuf[37], swapbuf[38], swapbuf[39],
           swapbuf[40], swapbuf[41], swapbuf[42], swapbuf[43], swapbuf[44], swapbuf[45], swapbuf[46], swapbuf[47]);
    printf("    %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[48], swapbuf[49], swapbuf[50], swapbuf[51], swapbuf[52], swapbuf[53], swapbuf[54], swapbuf[55],
           swapbuf[56], swapbuf[57], swapbuf[58], swapbuf[59], swapbuf[60], swapbuf[61], swapbuf[62], swapbuf[63]);

    memcpy(swapbuf, X, 64);
    BYTE_SWAP_BUFFER(swapbuf, 64);
    printf("X = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[ 0], swapbuf[ 1], swapbuf[ 2], swapbuf[ 3], swapbuf[ 4], swapbuf[ 5], swapbuf[ 6], swapbuf[ 7],
           swapbuf[ 8], swapbuf[ 9], swapbuf[10], swapbuf[11], swapbuf[12], swapbuf[13], swapbuf[14], swapbuf[15]);
    printf("    %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[16], swapbuf[17], swapbuf[18], swapbuf[19], swapbuf[20], swapbuf[21], swapbuf[22], swapbuf[23],
           swapbuf[24], swapbuf[25], swapbuf[26], swapbuf[27], swapbuf[28], swapbuf[29], swapbuf[30], swapbuf[31]);
    printf("    %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[32], swapbuf[33], swapbuf[34], swapbuf[35], swapbuf[36], swapbuf[37], swapbuf[38], swapbuf[39],
           swapbuf[40], swapbuf[41], swapbuf[42], swapbuf[43], swapbuf[44], swapbuf[45], swapbuf[46], swapbuf[47]);
    printf("    %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
           swapbuf[48], swapbuf[49], swapbuf[50], swapbuf[51], swapbuf[52], swapbuf[53], swapbuf[54], swapbuf[55],
           swapbuf[56], swapbuf[57], swapbuf[58], swapbuf[59], swapbuf[60], swapbuf[61], swapbuf[62], swapbuf[63]);
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
    BN_mod_exp(bn_seed, bn_Y, bn_K, bn_N, ctx);

    /* Apply server seed for establishing crypt state with client */
    uint8_t keybuf[64];
    BN_bn2bin(bn_seed, reinterpret_cast<unsigned char*>(keybuf));
    BYTE_SWAP_BUFFER(keybuf, 64);
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
    delete reinterpret_cast<CryptState_Private*>(state);
}

void DS::CryptSendBuffer(const DS::SocketHandle sock, DS::CryptState crypt,
                         const void* buffer, size_t size)
{
#ifdef DEBUG
    if (s_commdebug) {
        pthread_mutex_lock(&s_commdebug_mutex);
        printf("SEND TO %s", DS::SockIpAddress(sock).c_str());
        for (size_t i=0; i<size; ++i) {
            if ((i % 16) == 0)
                printf("\n    ");
            else if ((i % 16) == 8)
                printf("   ");
            printf("%02X ", reinterpret_cast<const uint8_t*>(buffer)[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&s_commdebug_mutex);
    }
#endif

    CryptState_Private* statep = reinterpret_cast<CryptState_Private*>(crypt);
    if (size > 4096) {
        unsigned char* cryptbuf = new unsigned char[size];
        RC4(&statep->m_writeKey, size, reinterpret_cast<const unsigned char*>(buffer), cryptbuf);
        DS::SendBuffer(sock, cryptbuf, size);
        delete[] cryptbuf;
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
    if (size > 4096) {
        unsigned char* cryptbuf = new unsigned char[size];
        DS::RecvBuffer(sock, cryptbuf, size);
        RC4(&statep->m_readKey, size, cryptbuf, reinterpret_cast<unsigned char*>(buffer));
        delete[] cryptbuf;
    } else {
        unsigned char stack[4096];
        DS::RecvBuffer(sock, stack, size);
        RC4(&statep->m_readKey, size, stack, reinterpret_cast<unsigned char*>(buffer));
    }

#ifdef DEBUG
    if (s_commdebug) {
        pthread_mutex_lock(&s_commdebug_mutex);
        printf("RECV FROM %s", DS::SockIpAddress(sock).c_str());
        for (size_t i=0; i<size; ++i) {
            if ((i % 16) == 0)
                printf("\n    ");
            else if ((i % 16) == 8)
                printf("   ");
            printf("%02X ", reinterpret_cast<const uint8_t*>(buffer)[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&s_commdebug_mutex);
    }
#endif
}

DS::ShaHash DS::BuggyHashPassword(const String& username, const String& password)
{
    StringBuffer<chr16_t> wuser = username.toUtf16();
    StringBuffer<chr16_t> wpass = password.toUtf16();
    chr16_t* buffer = new chr16_t[wuser.length() + wpass.length()];
    memcpy(buffer, wpass.data(), wpass.length() * sizeof(chr16_t));
    memcpy(buffer + wpass.length(), wuser.data(), wuser.length() * sizeof(chr16_t));
    buffer[wpass.length() - 1] = 0;
    buffer[wpass.length() + wuser.length() - 1] = 0;
    ShaHash hash = ShaHash::Sha0(buffer, (wuser.length() + wpass.length()) * sizeof(chr16_t));
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
