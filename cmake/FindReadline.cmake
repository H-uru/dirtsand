find_path(READLINE_INCLUDE_DIR readline.h
          /usr/include/readline
          /usr/local/include/readline
)

find_library(READLINE_LIBRARY
             NAMES readline libreadline
             PATH /usr/lib      /usr/local/lib
                  /usr/lib64    /usr/local/lib64
)

if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
    set(READLINE_FOUND TRUE)
endif()


if(READLINE_FOUND)
    if(NOT Readline_FIND_QUIETLY)
        message(STATUS "Found GNU Readline: ${READLINE_LIBRARY}")
    endif()
else()
   if(Readline_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find GNU Readline")
   endif()
endif()
