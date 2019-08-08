# This file is part of dirtsand.
#
# dirtsand is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# dirtsand is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.4)
project(dirtsand)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set up Product Identification parameters (NOTE: Should match plClient)
set(PRODUCT_BRANCH_ID   "1"         CACHE STRING "Branch ID")
set(PRODUCT_BUILD_ID    "918"       CACHE STRING "Build ID")
set(PRODUCT_BUILD_TYPE  "50"        CACHE STRING "Build Type")
set(PRODUCT_UUID        "ea489821-6c35-4bd0-9dae-bb17c585e680"
                                    CACHE STRING "Product UUID")

set(DS_HOOD_USER_NAME       "DS"
    CACHE STRING "Default Neighborhood Name")
set(DS_HOOD_INST_NAME       "Neighborhood"
    CACHE STRING "Default Neighborhood Instance Name")
set(DS_HOOD_POP_THRESHOLD   "20"
    CACHE STRING "Default Neighborhood Max Population")

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake")
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Readline REQUIRED)
find_package(ZLIB REQUIRED)
find_package(string_theory 2.0 REQUIRED)
find_package(Threads REQUIRED)

set(PlasMOUL_SOURCES
    PlasMOUL/AgeLinkStruct.cpp
    PlasMOUL/Key.cpp
    PlasMOUL/GenericValue.cpp
    PlasMOUL/Avatar/AnimStage.cpp
    PlasMOUL/Avatar/ArmatureBrain.cpp
    PlasMOUL/Avatar/AvBrainCoop.cpp
    PlasMOUL/Avatar/AvBrainGeneric.cpp
    PlasMOUL/Avatar/AvTask.cpp
    PlasMOUL/Avatar/CoopCoordinator.cpp
    PlasMOUL/Messages/EventData.cpp
    PlasMOUL/Messages/Message.cpp
    PlasMOUL/Messages/LoadCloneMsg.cpp
    PlasMOUL/Messages/LoadAvatarMsg.cpp
    PlasMOUL/Messages/AvatarInputStateMsg.cpp
    PlasMOUL/Messages/ServerReplyMsg.cpp
    PlasMOUL/Messages/NotifyMsg.cpp
    PlasMOUL/Messages/InputIfaceMgrMsg.cpp
    PlasMOUL/Messages/ClothingMsg.cpp
    PlasMOUL/Messages/LinkEffectsTriggerMsg.cpp
    PlasMOUL/Messages/KIMessage.cpp
    PlasMOUL/Messages/AvatarMsg.cpp
    PlasMOUL/Messages/AvTaskMsg.cpp
    PlasMOUL/Messages/AvSeekMsg.cpp
    PlasMOUL/Messages/EnableMsg.cpp
    PlasMOUL/Messages/BulletMsg.cpp
    PlasMOUL/Messages/SimulationMsg.cpp
    PlasMOUL/Messages/MessageWithCallbacks.cpp
    PlasMOUL/Messages/LinkToAgeMsg.cpp
    PlasMOUL/Messages/ParticleMsg.cpp
    PlasMOUL/Messages/MultistageMsg.cpp
    PlasMOUL/Messages/SetNetGroupIdMsg.cpp
    PlasMOUL/Messages/InputEventMsg.cpp
    PlasMOUL/Messages/WarpMsg.cpp
    PlasMOUL/Messages/CreatableList.cpp
    PlasMOUL/Messages/LinkingMgrMsg.cpp
    PlasMOUL/Messages/ClimbMsg.cpp
    PlasMOUL/Messages/BackdoorMsg.cpp
    PlasMOUL/Messages/PseudoLinkEffectMsg.cpp
    PlasMOUL/NetMessages/NetMessage.cpp
    PlasMOUL/NetMessages/NetMsgObject.cpp
    PlasMOUL/NetMessages/NetMsgGameMessage.cpp
    PlasMOUL/NetMessages/NetMsgLoadClone.cpp
    PlasMOUL/NetMessages/NetMsgPlayerPage.cpp
    PlasMOUL/NetMessages/NetMsgMembersList.cpp
    PlasMOUL/NetMessages/NetMsgRoomsList.cpp
    PlasMOUL/NetMessages/NetMsgGameState.cpp
    PlasMOUL/NetMessages/NetMsgSharedState.cpp
    PlasMOUL/NetMessages/NetMsgSDLState.cpp
    PlasMOUL/NetMessages/NetMsgGroupOwner.cpp
    PlasMOUL/NetMessages/NetMsgRelevanceRegions.cpp
    PlasMOUL/NetMessages/NetMsgVoice.cpp
    PlasMOUL/factory.cpp
)

set(SDL_SOURCES
    SDL/SdlParser.cpp
    SDL/DescriptorDb.cpp
    SDL/StateInfo.cpp
)

set(dirtsand_SOURCES
    Types/UnifiedTime.cpp
    Types/Uuid.cpp
    Types/ShaHash.cpp
    Types/BitVector.cpp
    Types/Math.cpp
    NetIO/MsgChannel.cpp
    NetIO/SockIO.cpp
    NetIO/CryptIO.cpp
    NetIO/Lobby.cpp
    NetIO/Status.cpp
    GateKeeper/GateServ.cpp
    FileServ/FileManifest.cpp
    FileServ/FileServer.cpp
    AuthServ/AuthManifest.cpp
    AuthServ/AuthServer.cpp
    AuthServ/AuthDaemon.cpp
    AuthServ/AuthVault.cpp
    AuthServ/VaultTypes.cpp
    GameServ/GameServer.cpp
    GameServ/GameHost.cpp
    streams.cpp
    settings.cpp
    dsmain.cpp
)

add_executable(dirtsand ${dirtsand_SOURCES} ${SDL_SOURCES} ${PlasMOUL_SOURCES})
set_target_properties(dirtsand PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "${dirtsand_SOURCE_DIR}/bin")

target_compile_definitions(dirtsand PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    ST_NO_3_0_DEPRECATION
    PRODUCT_BRANCH_ID=${PRODUCT_BRANCH_ID}
    PRODUCT_BUILD_ID=${PRODUCT_BUILD_ID}
    PRODUCT_BUILD_TYPE=${PRODUCT_BUILD_TYPE}
    PRODUCT_UUID="${PRODUCT_UUID}"
    HOOD_USER_NAME="${DS_HOOD_USER_NAME}"
    HOOD_INST_NAME="${DS_HOOD_INST_NAME}"
    HOOD_POP_THRESHOLD=${DS_HOOD_POP_THRESHOLD}
)

target_include_directories(dirtsand PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/PlasMOUL"
    "${PostgreSQL_INCLUDE_DIRS}"
    "${STRING_THEORY_INCLUDE_DIRS}"
)

target_link_libraries(dirtsand PRIVATE
    ${PostgreSQL_LIBRARIES}
    OpenSSL::Crypto
    ZLIB::ZLIB
    Readline::Readline
    ${STRING_THEORY_LIBRARIES}
    Threads::Threads
)

install(TARGETS dirtsand
        RUNTIME DESTINATION "bin")
install(PROGRAMS ${CMAKE_SOURCE_DIR}/bin/dsData.sh
        DESTINATION "bin")
install(FILES static_ages.ini
        DESTINATION ".")

# TODO: Build proper unit test framework...
option(ENABLE_TESTS "Enable building and running unit tests" ON)

# clang-tidy related rules and targets
include(TidyRules.cmake)

if(ENABLE_TESTS)
    add_executable(test_sha Types/ShaHash.cpp Types/ShaHash.h)
    target_compile_definitions(test_sha PRIVATE DS_BUILD_SHA0_TESTS)
    # Dependencies needed for ShaHash
    target_sources(test_sha PRIVATE streams.cpp streams.h errors.h)
    target_compile_definitions(test_sha PRIVATE ST_NO_3_0_DEPRECATION)
    target_include_directories(test_sha PRIVATE "${CMAKE_SOURCE_DIR}")
    target_link_libraries(test_sha PRIVATE ${OPENSSL_CRYPTO_LIBRARIES}
                                           ${STRING_THEORY_LIBRARIES})
endif()
