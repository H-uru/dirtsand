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
#include <openssl/bn.h>
#include <cstdio>

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
