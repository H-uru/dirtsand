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

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <catch2/catch.hpp>
#include <string_theory/format>

#include <unistd.h>

#include "SDL/DescriptorDb.h"
#include "SDL/StateInfo.h"

static const ST::string s_SdlDescriptor = ST_LITERAL(R"(
    STATEDESC Test
    {
        VERSION 1

        VAR BOOL    bTestVar1[1]    DEFAULT=0
        VAR BOOL    bTestVar2[1]    DEFAULT=1    DEFAULTOPTION=VAULT
        VAR INT     iTestVar3[1]    DEFAULT=0    DEFAULTOPTION=VAULT
        VAR INT     iTestVar4[1]    DEFAULT=100
    }

    STATEDESC Test
    {
        VERSION 2

        VAR BOOL    bTestVar1[1]    DEFAULT=0
        VAR BOOL    bTestVar2[1]    DEFAULT=1    DEFAULTOPTION=VAULT
        VAR INT     iTestVar3[1]    DEFAULT=0    DEFAULTOPTION=VAULT
        VAR INT     iTestVar4[1]    DEFAULT=100

        VAR BYTE    bTestVar5[1]    DEFAULT=50
    }

    STATEDESC Barney
    {
        VERSION 1
    }
)");

static bool LoadDescriptors()
{
    static bool result = false;
    if (result)
        return true;

    ST::string sdlFilePath;
    char tempDir[256] = "/tmp/DirtSandSDLTestXXXXXX";
    char* tempDirResult = nullptr;
    do {
        tempDirResult = mkdtemp(tempDir);
        if (!tempDirResult) {
            fprintf(stderr, "Failed to create a temporary directory for SDL.\n");
            break;
        }

        sdlFilePath = ST::format("{}/Test.sdl", tempDirResult);
        FILE* f = fopen(sdlFilePath.c_str(), "w+");
        if (!f) {
            fprintf(stderr, "Failed to create a file in our temporary SDL directory.\n");
            break;
        }
        fwrite(s_SdlDescriptor.c_str(), sizeof(char), s_SdlDescriptor.size(), f);
        fclose(f);

        SDL::DescriptorDb::LoadDescriptors(tempDirResult);
        result = true;
    } while (0);

    if (!sdlFilePath.empty())
        unlink(sdlFilePath.c_str());
    if (tempDirResult)
        rmdir(tempDirResult);
    return result;
}

#define CHECK_VAR_DESCRIPTOR(desc, name, varType, varMember, defaultValue)                      \
    SECTION("VarDescriptor (" #desc ")" name) {                                                 \
        auto varIt = desc->m_varmap.find(name);                                                 \
        REQUIRE(varIt != desc->m_varmap.end());                                                 \
                                                                                                \
        const SDL::VarDescriptor& var = desc->m_vars[varIt->second];                            \
                                                                                                \
        CHECK(var.m_name == name);                                                              \
        CHECK(var.m_type == varType);                                                           \
        CHECK(var.m_default.m_valid);                                                           \
        CHECK(var.m_default.varMember == defaultValue);                                         \
    }

#define CHECK_VALUE(varA, varB, member)                                                        \
    {                                                                                           \
        size_t compareSize = std::min(varA.data()->m_size, varB.data()->m_size);                \
        for (size_t i = 0; i < compareSize; ++i) {                                              \
            CHECK(varA.data()->member[i] == varB.data()->member[i]);                            \
        }                                                                                       \
    }

#define CHECK_VAR_VALUES(stateA, stateB, var)                                                   \
    SECTION("Var " var) {                                                                       \
        auto aVarIt = stateA.descriptor()->m_varmap.find(var);                                  \
        auto bVarIt = stateB.descriptor()->m_varmap.find(var);                                  \
        REQUIRE(aVarIt != stateA.descriptor()->m_varmap.end());                                 \
        REQUIRE(bVarIt != stateB.descriptor()->m_varmap.end());                                 \
                                                                                                \
        const SDL::Variable& varA = stateA.data()->m_vars[aVarIt->second];                      \
        const SDL::Variable& varB = stateB.data()->m_vars[bVarIt->second];                      \
        /* NOTE: SDL Variables could change types between versions, but no one does that. */    \
        REQUIRE(varA.descriptor()->m_type == varB.descriptor()->m_type);                        \
        switch (varA.descriptor()->m_type) {                                                    \
            case SDL::e_VarInt: CHECK_VALUE(varA, varB, m_int) break;                           \
            case SDL::e_VarFloat: CHECK_VALUE(varA, varB, m_float) break;                       \
            case SDL::e_VarBool: CHECK_VALUE(varA, varB, m_bool) break;                         \
            case SDL::e_VarString: CHECK_VALUE(varA, varB, m_string) break;                     \
            case SDL::e_VarKey: CHECK_VALUE(varA, varB, m_key)  break;                          \
            /* Skipping e_VarCreatable (unusued) */                                             \
            case SDL::e_VarCreatable: break;                                                    \
            case SDL::e_VarDouble: CHECK_VALUE(varA, varB, m_double) break;                     \
            case SDL::e_VarTime: CHECK_VALUE(varA, varB, m_time) break;                         \
            case SDL::e_VarByte: CHECK_VALUE(varA, varB, m_byte) break;                         \
            case SDL::e_VarShort: CHECK_VALUE(varA, varB, m_short) break;                       \
            /* Skipping e_VarAgeTimeofDay (no value) */                                         \
            case SDL::e_VarAgeTimeOfDay: break;                                                 \
            case SDL::e_VarVector3:                                                             \
            case SDL::e_VarPoint3:                                                              \
                CHECK_VALUE(varA, varB, m_vector)                                               \
                break;                                                                          \
            case SDL::e_VarQuaternion: CHECK_VALUE(varA, varB, m_quat) break;                   \
            case SDL::e_VarRgb:                                                                 \
            case SDL::e_VarRgba:                                                                \
                CHECK_VALUE(varA, varB, m_color)                                                \
                break;                                                                          \
            case SDL::e_VarRgb8:                                                                \
            case SDL::e_VarRgba8:                                                               \
                CHECK_VALUE(varA, varB, m_color8)                                               \
                break;                                                                          \
            /* Skipping e_VarStateDesc for now... */                                            \
            case SDL::e_VarStateDesc: break;                                                    \
            default: CHECK(0); break;                                                           \
        }                                                                                       \
}

static SDL::State CreateState()
{
    SDL::StateDescriptor* desc = SDL::DescriptorDb::FindDescriptor("Test", 1);
    REQUIRE(desc != nullptr);

    auto boolVarIdx = desc->m_varmap.find("bTestVar1");
    auto intVarIdx = desc->m_varmap.find("iTestVar3");

    REQUIRE(boolVarIdx != desc->m_varmap.end());
    REQUIRE(intVarIdx != desc->m_varmap.end());

    SDL::State state(desc);
    state.data()->m_vars[boolVarIdx->second].data()->m_bool[0] = true;
    state.data()->m_vars[intVarIdx->second].data()->m_int[0] = 6;
    return state;
}

TEST_CASE("Test SDL", "[sdl]")
{
    // Not really a test, but we do need to load the descriptors
    REQUIRE(LoadDescriptors());

    SECTION("SDL Descriptors") {
        SDL::StateDescriptor* v1 = SDL::DescriptorDb::FindDescriptor("Test", 1);
        SDL::StateDescriptor* v2 = SDL::DescriptorDb::FindDescriptor("Test", 2);

        REQUIRE(v1 != nullptr);
        CHECK(v1->m_version == 1);
        CHECK(v1->m_name == "Test");
        CHECK(v1->m_varmap.size() == 4);
        CHECK(v1->m_vars.size() == 4);
        CHECK_VAR_DESCRIPTOR(v1, "bTestVar1", SDL::e_VarBool, m_bool, false);
        CHECK_VAR_DESCRIPTOR(v1, "bTestVar2", SDL::e_VarBool, m_bool, true);
        CHECK_VAR_DESCRIPTOR(v1, "iTestVar3", SDL::e_VarInt, m_int, 0);
        CHECK_VAR_DESCRIPTOR(v1, "iTestVar4", SDL::e_VarInt, m_int, 100);

        REQUIRE(v2 != nullptr);
        CHECK(v2 != v1);
        CHECK(v2->m_version == 2);
        CHECK(v2->m_name == "Test");
        CHECK(v2->m_varmap.size() == 5);
        CHECK(v2->m_vars.size() == 5);
        CHECK_VAR_DESCRIPTOR(v2, "bTestVar1", SDL::e_VarBool, m_bool, false);
        CHECK_VAR_DESCRIPTOR(v2, "bTestVar2", SDL::e_VarBool, m_bool, true);
        CHECK_VAR_DESCRIPTOR(v2, "iTestVar3", SDL::e_VarInt, m_int, 0);
        CHECK_VAR_DESCRIPTOR(v2, "iTestVar4", SDL::e_VarInt, m_int, 100);
        CHECK_VAR_DESCRIPTOR(v2, "bTestVar5", SDL::e_VarByte, m_int, 50);
    }

    SECTION("SDL Blob Round Tripping") {
        SDL::State origState = CreateState();
        DS::Blob origBlob = origState.toBlob();
        SDL::State newState = SDL::State::FromBlob(origBlob);
        DS::Blob newBlob = newState.toBlob();

        CHECK(origBlob.size() == newBlob.size());
        CHECK(memcmp(newBlob.buffer(), origBlob.buffer(), origBlob.size()) == 0);
    }

    SECTION("SDL Blob Upgrade") {
        SDL::State origState = CreateState();
        SDL::State newState = origState;
        CHECK(newState.update());
        CHECK_VAR_VALUES(origState, newState, "bTestVar1");
        CHECK_VAR_VALUES(origState, newState, "iTestVar3");
    }
}
