# Locate the MySQL / MariaDB C client library and headers.
# Defines the imported target MySQL::Client.
#
# The code includes both <mysql.h> and <mysql/mysql.h> depending on macros, so we
# expose BOTH the mysql include dir and its parent so either spelling resolves.

set(_MYSQL_INC_HINTS "")
set(_MYSQL_LIB_HINTS "")

# Prefer mysql_config / mariadb_config when present (most reliable).
find_program(MYSQL_CONFIG NAMES mysql_config mariadb_config)
if(MYSQL_CONFIG)
    execute_process(COMMAND ${MYSQL_CONFIG} --include
        OUTPUT_VARIABLE _mc_inc OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${MYSQL_CONFIG} --libs
        OUTPUT_VARIABLE _mc_libs OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(REGEX MATCHALL "-I[^ ]+" _incs "${_mc_inc}")
    foreach(i ${_incs})
        string(SUBSTRING "${i}" 2 -1 i)
        list(APPEND _MYSQL_INC_HINTS "${i}")
    endforeach()
    string(REGEX MATCHALL "-L[^ ]+" _libdirs "${_mc_libs}")
    foreach(l ${_libdirs})
        string(SUBSTRING "${l}" 2 -1 l)
        list(APPEND _MYSQL_LIB_HINTS "${l}")
    endforeach()
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_MYSQL QUIET libmariadb mariadb libmysqlclient mysqlclient)
    list(APPEND _MYSQL_INC_HINTS ${PC_MYSQL_INCLUDEDIR} ${PC_MYSQL_INCLUDE_DIRS})
    list(APPEND _MYSQL_LIB_HINTS ${PC_MYSQL_LIBDIR} ${PC_MYSQL_LIBRARY_DIRS})
endif()

find_path(MYSQL_INCLUDE_DIR
    NAMES mysql.h
    HINTS ${_MYSQL_INC_HINTS}
    PATH_SUFFIXES mysql mariadb)

find_library(MYSQL_LIBRARY
    NAMES mariadbclient mariadb mysqlclient libmysql
    HINTS ${_MYSQL_LIB_HINTS}
    PATH_SUFFIXES mysql mariadb)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQLClient
    REQUIRED_VARS MYSQL_LIBRARY MYSQL_INCLUDE_DIR)

if(MySQLClient_FOUND AND NOT TARGET MySQL::Client)
    # Also expose the parent dir so <mysql/mysql.h> resolves when the headers
    # live in <prefix>/include/mysql/.
    get_filename_component(_mysql_parent "${MYSQL_INCLUDE_DIR}" DIRECTORY)
    add_library(MySQL::Client UNKNOWN IMPORTED)
    set_target_properties(MySQL::Client PROPERTIES
        IMPORTED_LOCATION "${MYSQL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MYSQL_INCLUDE_DIR};${_mysql_parent}")
endif()

mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARY MYSQL_CONFIG)
