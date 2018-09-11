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

find_path(Readline_INCLUDE_DIR readline.h
          /usr/include/readline
          /usr/local/include/readline
)

find_library(Readline_LIBRARY
             NAMES readline libreadline
             PATH /usr/lib      /usr/local/lib
                  /usr/lib64    /usr/local/lib64
)

mark_as_advanced(Readline_INCLUDE_DIR Readline_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Readline
    REQUIRED_VARS Readline_LIBRARY Readline_INCLUDE_DIR
)

if(Readline_FOUND AND NOT TARGET Readline::Readline)
    add_library(Readline::Readline UNKNOWN IMPORTED)
    set_target_properties(Readline::Readline PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${Readline_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Readline_INCLUDE_DIR}"
    )
endif()
