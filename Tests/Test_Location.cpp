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

#include "PlasMOUL/Key.h"

#define CHECK_SEQUENCE(loc, sequence) \
    CHECK((loc).m_sequence == sequence)

TEST_CASE("Test MOUL::Location", "[location]")
{
    SECTION("From prefix and page number") {
        // Yes, there are multiple ways of encoding the same sequence...
        CHECK_SEQUENCE(MOUL::Location(0, 0, 0), 0x00000021);
        CHECK_SEQUENCE(MOUL::Location(1, 0, 0), 0x00010021);
        CHECK_SEQUENCE(MOUL::Location(100, 1, 0), 0x00640022);
        CHECK_SEQUENCE(MOUL::Location(65535, 65502, 0), 0xFFFFFFFF);

        CHECK_SEQUENCE(MOUL::Location(1, -1, 0), 0x00020020);
        CHECK_SEQUENCE(MOUL::Location(100, -33, 0), 0x00650000);
        CHECK_SEQUENCE(MOUL::Location(65534, -33, 0), 0xFFFF0000);

        CHECK_SEQUENCE(MOUL::Location(-1, 0, 0), 0xFF010001);
        CHECK_SEQUENCE(MOUL::Location(-100, 1, 0), 0xFF640002);
        CHECK_SEQUENCE(MOUL::Location(-255, 65534, 0), 0xFFFFFFFF);

        CHECK_SEQUENCE(MOUL::Location(-1, -1, 0), 0xFF020000);
        CHECK_SEQUENCE(MOUL::Location(-254, -1, 0), 0xFFFF0000);

        // Wrap around -- not actually valid...
        CHECK_SEQUENCE(MOUL::Location(65537, 0, 0), 0x00010021);
        CHECK_SEQUENCE(MOUL::Location(65536, -1, 0), 0x00010020);
        CHECK_SEQUENCE(MOUL::Location(65536, -33, 0), 0x00010000);
        CHECK_SEQUENCE(MOUL::Location(-256, 0, 0), 0x00000001);
        CHECK_SEQUENCE(MOUL::Location(-255, -1, 0), 0x00000000);

        CHECK_SEQUENCE(MOUL::Location(1, 65503, 0), 0x00020000);
        CHECK_SEQUENCE(MOUL::Location(1, -34, 0), 0x0001FFFF);
        CHECK_SEQUENCE(MOUL::Location(-255, 65535, 0), 0x00000000);
        CHECK_SEQUENCE(MOUL::Location(-255, -2, 0), 0xFFFFFFFF);
    }
}
