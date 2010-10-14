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

include_directories("${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/PlasMOUL")

set(PlasMOUL_SOURCES
    PlasMOUL/Key.cpp
    PlasMOUL/NetMessages/NetMessage.cpp
    PlasMOUL/NetMessages/PlayerPage.cpp
    PlasMOUL/factory.cpp
)

set(dirtsand_SOURCES
    Types/UnifiedTime.cpp
    Types/Uuid.cpp
    NetIO/MsgChannel.cpp
    NetIO/SockIO.cpp
    NetIO/Lobby.cpp
    strings.cpp
    streams.cpp
    settings.cpp
    encodings.cpp
    dsmain.cpp
)

add_executable(dirtsand ${PlasMOUL_SOURCES} ${dirtsand_SOURCES})
target_link_libraries(dirtsand ${CMAKE_THREAD_LIBS_INIT} ${POSTGRES_LIBRARY})
set_target_properties(dirtsand PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "${dirtsand_SOURCE_DIR}/bin")

install(TARGETS dirtsand
        RUNTIME DESTINATION "bin")
