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

#include "streams.h"

#include <catch2/catch.hpp>
#include <tuple>

TEST_CASE("EncryptedStream known values", "[streams]")
{
    const char result[] = "Hello, world!";
    constexpr size_t resultsz = sizeof(result) - 1;

    SECTION("xxtea") {
        uint32_t keys[] = { 0x31415926, 0x53589793, 0x23846264, 0x33832795 };
        uint8_t buff[] = {
            0x6E, 0x6F, 0x74, 0x74, 0x68, 0x65, 0x64, 0x72, 0x6F, 0x69, 0x64, 0x73,
            0x0D, 0x00, 0x00, 0x00, 0x93, 0xBD, 0x71, 0x93, 0xA4, 0x40, 0xC2, 0x6A,
            0x37, 0xD1, 0xA7, 0x9E, 0xEA, 0x93, 0x45, 0xC9
        };

        DS::BufferStream base(buff, sizeof(buff));
        DS::EncryptedStream stream(&base, DS::EncryptedStream::Mode::e_read, std::nullopt, keys);
        REQUIRE(stream.getEncType() == DS::EncryptedStream::Type::e_xxtea);
        REQUIRE(stream.size() == resultsz);

        REQUIRE_FALSE(stream.atEof());

        char test[sizeof(result)];
        stream.readBytes(test, resultsz);
        test[sizeof(test) - 1] = 0;
        CAPTURE(test);
        REQUIRE(memcmp(test, result, resultsz) == 0);

        REQUIRE(stream.atEof());
    }

    SECTION("tea") {
        uint8_t buff[] = {
            0x77, 0x68, 0x61, 0x74, 0x64, 0x6F, 0x79, 0x6F, 0x75, 0x73, 0x65, 0x65,
            0x0D, 0x00, 0x00, 0x00, 0xAC, 0xC1, 0xA6, 0xB6, 0xDC, 0x33, 0x95, 0x0E,
            0x99, 0x18, 0xAE, 0xFC, 0x9C, 0xD3, 0x00, 0xB9
        };

        DS::BufferStream base(buff, sizeof(buff));
        DS::EncryptedStream stream(&base, DS::EncryptedStream::Mode::e_read);
        REQUIRE(stream.getEncType() == DS::EncryptedStream::Type::e_tea);
        REQUIRE(stream.size() == resultsz);

        REQUIRE_FALSE(stream.atEof());

        char test[sizeof(result)];
        stream.readBytes(test, resultsz);
        test[sizeof(test) - 1] = 0;
        CAPTURE(test);
        REQUIRE(memcmp(test, result, resultsz) == 0);

        REQUIRE(stream.atEof());
    }
}

#define WRITE_STRING(_stream, _str)                                           \
    _stream.writeBytes(_str, sizeof(_str) - 1);

#define CHECK_STRING(_stream, _str)                                           \
    {                                                                         \
        constexpr size_t bufsz = sizeof(_str) - 1;                            \
        uint8_t buf[bufsz];                                                   \
        _stream.readBytes(buf, bufsz);                                        \
        REQUIRE(memcmp(buf, _str, bufsz) == 0);                               \
    }                                                                         //

TEST_CASE("EncryptedStream round-trip", "[streams]",) {
    auto type = GENERATE(
        DS::EncryptedStream::Type::e_xxtea,
        DS::EncryptedStream::Type::e_tea
    );

    DS::BufferStream base;
    {
        DS::EncryptedStream stream(&base, DS::EncryptedStream::Mode::e_write, type);
        WRITE_STRING(stream, "Small"); // Purposefully take up less than a full block
        WRITE_STRING(stream, "!! "); // Complete the block from the previous write
        WRITE_STRING(stream, "BlockSZ!"); // A full block
        WRITE_STRING(stream, " ... And finally, something longer than a single block!");
    }
    base.seek(0, SEEK_SET);
    {
        DS::EncryptedStream stream(&base, DS::EncryptedStream::Mode::e_read);
        CHECK_STRING(stream, "Small");
        CHECK_STRING(stream, "!! ");
        CHECK_STRING(stream, "BlockSZ!");
        CHECK_STRING(stream, " ... And finally, something longer than a single block!");
    }
    base.seek(0, SEEK_SET);
    {
        DS::EncryptedStream stream(&base, DS::EncryptedStream::Mode::e_read);
        CHECK_STRING(stream, "Small!! BlockSZ! ... And finally, something longer than a single block!");
    }
}

TEST_CASE("EncryptedStream Magic Strings", "[streams]")
{
    SECTION("whatdoyousee") {
        const char buffer[] { "whatdoyousee\x0\x0\x0\x0" };
        DS::BufferStream base(buffer, sizeof(buffer));
        REQUIRE(DS::EncryptedStream::CheckEncryption(&base) == DS::EncryptedStream::Type::e_tea);
    }

    SECTION("BriceIsSmart") {
        const char buffer[] { "BriceIsSmart\x0\x0\x0\x0" };
        DS::BufferStream base(buffer, sizeof(buffer));
        REQUIRE(DS::EncryptedStream::CheckEncryption(&base) == DS::EncryptedStream::Type::e_tea);
    }

    SECTION("notthedroids") {
        const char buffer[] { "notthedroids\x0\x0\x0\x0" };
        DS::BufferStream base(buffer, sizeof(buffer));
        REQUIRE(DS::EncryptedStream::CheckEncryption(&base) == DS::EncryptedStream::Type::e_xxtea);
    }
}
