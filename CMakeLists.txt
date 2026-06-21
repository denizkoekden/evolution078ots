###############################################################################
# OpenTibia Server "Evolution 0.7.8" — modern, portable CMake build
#
# Goal: build the ORIGINAL source on modern GCC / Clang / MSVC across
#       Windows, Linux and macOS, with 1:1 runtime behaviour preserved.
#
# The authoritative build manifest is source/dev-C++/Makefile.win (the original
# Dev-C++ project). The autotools files (Makefile.am / configure.in / ...) are
# stale and broken for this fork and are intentionally NOT used.
#
# Storage backend is a COMPILE-TIME choice in the original code (USE_SQL_ENGINE
# selects the SQL IO classes, otherwise the XML IO classes are used). We expose
# this via -DSTORAGE=... :
#     sqlite        (default) self-contained, "download & run"
#     mysql         original 1:1 backend (needs an external MySQL server)
#     mysql+sqlite  one binary, runtime choice via config.lua sql_type
#     xml           nostalgic original file-based storage (no SQL at all)
###############################################################################
cmake_minimum_required(VERSION 3.18)

project(EvolutionServer
    VERSION 0.7.8
    DESCRIPTION "OpenTibia Server Evolution 0.7.8 (modernized build)"
    LANGUAGES C CXX)

# --- Standard ---------------------------------------------------------------
# Original was GCC 3.4.2 (C++03 + GNU extensions). gnu++14 is the minimal
# modern standard that keeps the GNU extensions the code relies on while
# letting us drop the removed-construct workarounds.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)            # -> gnu++14
set(CMAKE_C_STANDARD 99)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Help local (Homebrew) discovery on macOS.
if(APPLE)
    foreach(_p boost gmp libxml2 sqlite mysql-client luajit)
        execute_process(COMMAND brew --prefix ${_p}
            OUTPUT_VARIABLE _bp OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_bp AND EXISTS "${_bp}")
            list(APPEND CMAKE_PREFIX_PATH "${_bp}")
        endif()
    endforeach()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# --- Options ----------------------------------------------------------------
set(STORAGE "sqlite" CACHE STRING "Storage backend: sqlite | mysql | mysql+sqlite | xml")
set_property(CACHE STORAGE PROPERTY STRINGS sqlite mysql "mysql+sqlite" xml)

set(LUA_PROVIDER "vendored" CACHE STRING "Lua provider: vendored (5.1 from repo) | luajit | system")
set_property(CACHE LUA_PROVIDER PROPERTY STRINGS vendored luajit system)

option(WARNINGS_AS_ERRORS "Treat warnings as errors (-Werror)" OFF)

# Decode STORAGE -> feature flags
set(USE_MYSQL  OFF)
set(USE_SQLITE OFF)
set(USE_XML    OFF)
if(STORAGE STREQUAL "sqlite")
    set(USE_SQLITE ON)
elseif(STORAGE STREQUAL "mysql")
    set(USE_MYSQL ON)
elseif(STORAGE STREQUAL "mysql+sqlite")
    set(USE_MYSQL ON)
    set(USE_SQLITE ON)
elseif(STORAGE STREQUAL "xml")
    set(USE_XML ON)
else()
    message(FATAL_ERROR "Invalid STORAGE='${STORAGE}'. Use sqlite | mysql | mysql+sqlite | xml")
endif()
message(STATUS "Evolution: STORAGE=${STORAGE}  LUA_PROVIDER=${LUA_PROVIDER}")

###############################################################################
# Source manifest (mirrors source/dev-C++/Makefile.win exactly)
###############################################################################
set(SRC_DIR "${CMAKE_SOURCE_DIR}/source")

# 63 always-compiled translation units (Makefile.win minus the backend-specific
# ones). iomapserializexml/iomapxml/iomapotbm are always built, as in the original.
set(SRC_COMMON
    account actions admin allocator ban baseevents beds chat combat commands
    condition configmanager container creature cylinder depot exception
    fileloader game guilds house housetile ioaccount iomapotbm iomapserialize
    iomapserializexml iomapxml ioplayer item itemloader items logger luascript
    mailbox map monster monsters movement networkmessage npc otserv outfit party
    player position protocol79 pvparena raids rsa scheduler scriptmanager spawn
    spells status talkaction teleport thing tile tools trashholder vocation
    waitlist weapons)

# Backend-specific units
set(SRC_SQL_CORE database ioaccountsql ioplayersql iomapserializesql) # any SQL engine
set(SRC_MYSQL    databasemysql)
set(SRC_SQLITE   databasesqlite)
set(SRC_XML      ioaccountxml ioplayerxml)

set(SOURCES "")
foreach(u ${SRC_COMMON})
    list(APPEND SOURCES "${SRC_DIR}/${u}.cpp")
endforeach()

if(USE_MYSQL OR USE_SQLITE)            # any SQL engine -> SQL IO classes
    foreach(u ${SRC_SQL_CORE})
        list(APPEND SOURCES "${SRC_DIR}/${u}.cpp")
    endforeach()
    if(USE_MYSQL)
        list(APPEND SOURCES "${SRC_DIR}/${SRC_MYSQL}.cpp")
    endif()
    if(USE_SQLITE)
        list(APPEND SOURCES "${SRC_DIR}/${SRC_SQLITE}.cpp")
    endif()
else()                                 # XML build -> XML IO classes
    foreach(u ${SRC_XML})
        list(APPEND SOURCES "${SRC_DIR}/${u}.cpp")
    endforeach()
endif()

list(LENGTH SOURCES _nsrc)
message(STATUS "Evolution: compiling ${_nsrc} translation units")

add_executable(evolutions ${SOURCES})
target_include_directories(evolutions PRIVATE "${SRC_DIR}")

###############################################################################
# Preprocessor defines (mirrors Makefile.win CXXFLAGS, 1:1)
###############################################################################
# Evolution feature flags — part of the server's behaviour, must stay defined.
target_compile_definitions(evolutions PRIVATE
    __SKULLSYSTEM__ __PARTYSYSTEM__ __DEBUG_PLAYERS__ __XID_BUY_SELL__
    __TLM_SERVER_SAVE__ __XID_CVS_MODS__ __XID_PREMIUM_SYSTEM__ __PB_BUY_HOUSE__
    __XID_LEAVE_HOUSE__ __YUR_GUILD_SYSTEM__ __TR_ANTI_AFK__ __XID_CMD_EXT__
    __TC_BROADCAST_COLORS__ __XID_CTRL_Z__ __SILV_MC_CHECK__ __YUR_SHUTDOWN__
    __YUR_CLEAN_MAP__ __XID_ROOKGARD__ __TC_GM_INVISIBLE__ __XID_LEARN_SPELLS__
    __JD_DEATH_LIST__ __XID_CONFIG_CAP__ __XID_PREVENT_LOSS__ __UCB_ONLINE_LIST__
    __XID_ACCOUNT_MANAGER__ __XID_CTRL_Y__ __XID_ADD_SKILLLEVEL__
    __XID_SUMMONS_FOLLOW__ __XID_PVP_FEATURES__ __XID_PROTECTION_SYSTEM__
    __JD_BED_SYSTEM__ __XID_SEPERATE_ADDONS__ __NFS_PVP_ARENA__
    __XID_EXPERIENCE_STAGES__ __XID_BLESS_SYSTEM__)

# Assumption (stated): load ./config.lua on every OS (original *nix used
# $HOME/.otserv/). Makes the packages "download & run".
target_compile_definitions(evolutions PRIVATE __NO_HOMEDIR_CONF__)

# Backend selection macros (drive USE_SQL_ENGINE in definitions.h)
if(USE_MYSQL)
    target_compile_definitions(evolutions PRIVATE __USE_MYSQL__)
endif()
if(USE_SQLITE)
    target_compile_definitions(evolutions PRIVATE __USE_SQLITE__)
endif()

# Platform macros — define the full set the code branches on, consistently.
if(WIN32)
    target_compile_definitions(evolutions PRIVATE __WINDOWS__ WIN32 _WIN32
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_definitions(evolutions PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    # Original Evolution application icon (as embedded by the Dev-C++ build via
    # Evolutions-SQL_private.rc). Copy the .ico next to a generated .rc in the build
    # tree and reference it by filename, so the RC compiler resolves it reliably.
    enable_language(RC)
    configure_file("${SRC_DIR}/dev-C++/Evolutions-SQL.ico"
                   "${CMAKE_BINARY_DIR}/evolution.ico" COPYONLY)
    file(WRITE "${CMAKE_BINARY_DIR}/evolution.rc" "1 ICON \"evolution.ico\"\n")
    target_sources(evolutions PRIVATE "${CMAKE_BINARY_DIR}/evolution.rc")
endif()

# Conservative 1:1 safety: the original GCC 3.4 did not do aggressive
# strict-aliasing optimization. The code type-puns the wire protocol buffers;
# disable strict-aliasing project-wide so behaviour matches the original until
# the memcpy remediation (step 8) lands.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(evolutions PRIVATE -fno-strict-aliasing)
endif()

if(WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(evolutions PRIVATE /WX)
    else()
        target_compile_options(evolutions PRIVATE -Werror)
    endif()
endif()

###############################################################################
# Dependencies
###############################################################################
# --- libxml2 (always) -------------------------------------------------------
find_package(LibXml2 REQUIRED)
target_link_libraries(evolutions PRIVATE LibXml2::LibXml2)

# --- Boost (always: regex compiled; bind/function/tokenizer/pool header-only)-
find_package(Boost REQUIRED COMPONENTS regex)
target_link_libraries(evolutions PRIVATE Boost::regex Boost::boost)
# No call site uses boost::bind placeholders -> silence the global-placeholder
# deprecation without changing behaviour.
target_compile_definitions(evolutions PRIVATE BOOST_BIND_GLOBAL_PLACEHOLDERS)

# --- GMP (always: rsa.cpp) --------------------------------------------------
find_package(GMP REQUIRED)
target_link_libraries(evolutions PRIVATE GMP::GMP)

# --- Lua 5.1 ----------------------------------------------------------------
if(LUA_PROVIDER STREQUAL "vendored")
    include(BuildVendoredLua)        # defines target lua51 (PUC Lua 5.1, from repo tarball)
    target_link_libraries(evolutions PRIVATE lua51)
elseif(LUA_PROVIDER STREQUAL "luajit")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LUAJIT REQUIRED IMPORTED_TARGET luajit)
    target_link_libraries(evolutions PRIVATE PkgConfig::LUAJIT)
else() # system
    find_package(Lua 5.1 EXACT REQUIRED)
    target_include_directories(evolutions PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(evolutions PRIVATE ${LUA_LIBRARIES})
endif()

# --- SQLite (optional) ------------------------------------------------------
if(USE_SQLITE)
    find_package(SQLite3 REQUIRED)
    # Use the long-standing target name SQLite::SQLite3 (present on every CMake that
    # ships FindSQLite3; the SQLite3::SQLite3 alias only exists on very new CMake).
    target_link_libraries(evolutions PRIVATE SQLite::SQLite3)
endif()

# --- MySQL / MariaDB client (optional) --------------------------------------
if(USE_MYSQL)
    include(FindMySQLClient)         # defines target MySQL::Client
    target_link_libraries(evolutions PRIVATE MySQL::Client)
endif()

# --- Threads + platform system libs -----------------------------------------
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(evolutions PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(evolutions PRIVATE ws2_32)
else()
    target_link_libraries(evolutions PRIVATE ${CMAKE_DL_LIBS})
endif()

###############################################################################
# Install / packaging layout (binary + runtime data next to it)
###############################################################################
install(TARGETS evolutions RUNTIME DESTINATION .)
install(FILES "${CMAKE_SOURCE_DIR}/config.lua" DESTINATION .)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data" DESTINATION .)
if(USE_SQLITE)
    install(FILES "${SRC_DIR}/db.s3db" DESTINATION .)
endif()
if(USE_MYSQL)
    install(FILES "${CMAKE_SOURCE_DIR}/database.sql" DESTINATION .)
endif()
