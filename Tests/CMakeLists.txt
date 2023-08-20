include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.9
)
FetchContent_MakeAvailable(Catch2)

set(test_SOURCES
    main.cpp
    Test_EncryptedStream.cpp
    Test_Location.cpp
    Test_SDL.cpp
    Test_ShaHash.cpp
)
add_executable(test_dirtsand ${test_SOURCES})
target_link_libraries(test_dirtsand
    PRIVATE
        Catch2::Catch2
        dirtsand
)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/contrib")
include(CTest)
include(Catch)
catch_discover_tests(test_dirtsand)
