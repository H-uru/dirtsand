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

#include <catch2/catch.hpp>
#include <openssl/evp.h>

#include "Types/ShaHash.h"

#define EXPECT_SHA(sha, expr) \
    CHECK((expr).toString() == sha)

TEST_CASE("SHA-0 hashes", "[sha]")
{
    OpenSSL_add_all_digests();

    // Sanity check EXPECT_SHA with known-good SHA-1 implementation
    SECTION("EXPECT_SHA macro") {
        EXPECT_SHA(
            "da39a3ee5e6b4b0d3255bfef95601890afd80709",
            DS::ShaHash::Sha1("", 0)
        );
        EXPECT_SHA(
            "a9993e364706816aba3e25717850c26c9cd0d89d",
            DS::ShaHash::Sha1("abc", 3)
        );
    }

    // Now the SHA-0 tests
    SECTION("SHA-0 hashes") {
        EXPECT_SHA(
            "f96cea198ad1dd5617ac084a3d92c6107708c0ef",
            DS::ShaHash::Sha0("", 0)
        );
        EXPECT_SHA(
            "37f297772fae4cb1ba39b6cf9cf0381180bd62f2",
            DS::ShaHash::Sha0("a", 1)
        );
        EXPECT_SHA(
            "0164b8a914cd2a5e74c4f7ff082c4d97f1edf880",
            DS::ShaHash::Sha0("abc", 3)
        );
        EXPECT_SHA(
            "d2516ee1acfa5baf33dfc1c471e438449ef134c8",
            DS::ShaHash::Sha0(
                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56
            )
        );
        EXPECT_SHA(
            "459f83b95db2dc87bb0f5b513a28f900ede83237",
            DS::ShaHash::Sha0(
                "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
                112
            )
        );
    }
}
