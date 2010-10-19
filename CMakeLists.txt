project(dirtsand)
cmake_minimum_required(VERSION 2.6)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter ${CMAKE_CXX_FLAGS}")
endif()
set(CMAKE_C_FLAGS_DEBUG "-DDEBUG ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG}")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Threads REQUIRED)
find_package(Postgres REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Readline REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/PlasMOUL"
                    "${POSTGRES_INCLUDE_DIR}"
                    "${OPENSSL_INCLUDE_DIR}"
                    "${READLINE_INCLUDE_DIR}")

set(PlasMOUL_SOURCES
    PlasMOUL/Key.cpp
    PlasMOUL/NetMessages/NetMessage.cpp
    PlasMOUL/NetMessages/PlayerPage.cpp
    PlasMOUL/factory.cpp
)

set(dirtsand_SOURCES
    Types/UnifiedTime.cpp
    Types/Uuid.cpp
    Types/ShaHash.cpp
    NetIO/MsgChannel.cpp
    NetIO/SockIO.cpp
    NetIO/CryptIO.cpp
    NetIO/Lobby.cpp
    GateKeeper/GateServ.cpp
    FileServ/FileManifest.cpp
    FileServ/FileServer.cpp
    AuthServ/AuthServer.cpp
    AuthServ/AuthDaemon.cpp
    strings.cpp
    streams.cpp
    settings.cpp
    encodings.cpp
    dsmain.cpp
)

add_executable(dirtsand ${dirtsand_SOURCES} ${PlasMOUL_SOURCES})
target_link_libraries(dirtsand ${CMAKE_THREAD_LIBS_INIT} ${POSTGRES_LIBRARY}
                               ${OPENSSL_CRYPTO_LIBRARIES} ${READLINE_LIBRARY})
set_target_properties(dirtsand PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "${dirtsand_SOURCE_DIR}/bin")

install(TARGETS dirtsand
        RUNTIME DESTINATION "bin")
