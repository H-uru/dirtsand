project(dirtsand)
cmake_minimum_required(VERSION 2.6)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GCC_VERSION)
    if(GCC_VERSION VERSION_LESS 4.6)
        message(FATAL_ERROR "GCC version >= 4.6 required")
    endif()

    set(WARN_FLAGS "-Wall -Wextra -Wno-unused-parameter")
    set(CMAKE_C_FLAGS "${WARN_FLAGS} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-std=c++0x ${WARN_FLAGS} ${CMAKE_CXX_FLAGS}")
endif()
set(CMAKE_C_FLAGS_DEBUG "-DDEBUG ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG}")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Postgres REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Readline REQUIRED)
find_package(ZLIB REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/PlasMOUL"
                    "${POSTGRES_INCLUDE_DIR}"
                    "${OPENSSL_INCLUDE_DIR}"
                    "${READLINE_INCLUDE_DIR}"
                    "${ZLIB_INCLUDE_DIR}")

try_compile(HAVE_ATOMIC ${dirtsand_BINARY_DIR}
            ${dirtsand_SOURCE_DIR}/cmake/CheckAtomic.cpp
            OUTPUT_VARIABLE OUTPUT)
if(HAVE_ATOMIC)
    add_definitions(-DHAVE_ATOMIC)
endif()

set(PlasMOUL_SOURCES
    PlasMOUL/AgeLinkStruct.cpp
    PlasMOUL/Key.cpp
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
    strings.cpp
    streams.cpp
    settings.cpp
    encodings.cpp
    dsmain.cpp
)

add_executable(dirtsand ${dirtsand_SOURCES} ${SDL_SOURCES} ${PlasMOUL_SOURCES})
target_link_libraries(dirtsand ${POSTGRES_LIBRARY} ${ZLIB_LIBRARY}
                               ${OPENSSL_CRYPTO_LIBRARIES} ${READLINE_LIBRARY})
set_target_properties(dirtsand PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "${dirtsand_SOURCE_DIR}/bin")

install(TARGETS dirtsand
        RUNTIME DESTINATION "bin")
install(PROGRAMS ${CMAKE_SOURCE_DIR}/bin/dsData.sh
        DESTINATION "bin")
install(FILES static_ages.ini
        DESTINATION ".")
