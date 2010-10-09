project(dirtsand)
cmake_minimum_required(VERSION 2.6)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter ${CMAKE_CXX_FLAGS}")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/PlasMOUL")

set(PlasMOUL_SOURCES
    PlasMOUL/Messages/Message.cpp
    PlasMOUL/Key.cpp
    PlasMOUL/factory.cpp
)

set(dirtsand_SOURCES
    strings.cpp
    streams.cpp
    dstest.cpp
)

add_executable(dirtsand ${PlasMOUL_SOURCES} ${dirtsand_SOURCES})
set_target_properties(dirtsand PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "${dirtsand_SOURCE_DIR}/bin")

install(TARGETS dirtsand
        RUNTIME DESTINATION "bin")
