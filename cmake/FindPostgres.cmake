find_path(POSTGRES_INCLUDE_DIR postgres.h
          /usr/include/postgresql
          /usr/include/postgresql/server
          /usr/local/include/postgresql
          /usr/local/include/postgresql/server
)

find_library(POSTGRES_LIBRARY
             NAMES pq libpq
             PATH /usr/lib      /usr/local/lib
                  /usr/lib64    /usr/local/lib64
)

if(POSTGRES_INCLUDE_DIR AND POSTGRES_LIBRARY)
    set(POSTGRES_FOUND TRUE)
endif()


if(POSTGRES_FOUND)
    if(NOT Postgres_FIND_QUIETLY)
        message(STATUS "Found PosgreSQL: ${POSTGRES_LIBRARY}")
    endif()
else()
   if(Postgres_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find PostgreSQL")
   endif()
endif()
