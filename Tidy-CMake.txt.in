find_program(
    CLANG_TIDY_EXE
    NAMES "clang-tidy"
    DOC "Path to clang-tidy executable")

if(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found.")
else()
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    set(CLANG_TIDY_FIXIT_DIR ${CMAKE_BINARY_DIR}/fixits)
    set(CLANG_TIDY_FIXIT_FILE ${CLANG_TIDY_FIXIT_DIR}/ALL.yaml)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} "-p" ${CMAKE_BINARY_DIR} "-export-fixes=${CLANG_TIDY_FIXIT_FILE}" ${dirtsand_SOURCES} ${SDL_SOURCES} ${PlasMOUL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Analyzing with clang-tidy..."
    )

    add_custom_target(tidy-base)
    add_custom_target(tidy-make-fixit-dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CLANG_TIDY_FIXIT_DIR})
    add_custom_target(tidy-rm-fixit-dir COMMAND ${CMAKE_COMMAND} -E remove_directory ${CLANG_TIDY_FIXIT_DIR})
    add_dependencies(tidy-make-fixit-dir tidy-rm-fixit-dir)
    add_dependencies(tidy-base tidy-make-fixit-dir)
    add_dependencies(tidy tidy-base)

    find_program(
        CLANG_APPLY_EXE
        NAMES "clang-apply-replacements"
        DOC "Path to clang-apply-replacements executable")

    if(NOT CLANG_TIDY_EXE)
        message(STATUS "clang-apply-replacements not found. Fixes must be applied manually")
    else()
        add_custom_target(fix
            COMMAND ${CLANG_APPLY_EXE} ${CLANG_TIDY_FIXIT_DIR}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Applying clang-tidy fixes..."
        )
        add_dependencies(fix tidy)
    endif()

endif()

