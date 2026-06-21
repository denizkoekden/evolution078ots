# Build PUC-Rio Lua 5.1 as a static library from the tarball bundled in the
# repository (source/libraries/lua-5.1.tar.gz). Vendoring guarantees byte-for-byte
# identical Lua semantics on Windows, Linux and macOS — the strongest 1:1 guarantee
# — and avoids the fact that lua5.1 is no longer packaged on some platforms.
#
# Produces the target: lua51  (with its src/ dir as a SYSTEM include directory)

set(_LUA_TARBALL "${CMAKE_SOURCE_DIR}/source/libraries/lua-5.1.tar.gz")
set(_LUA_SHA256  "7f5bb9061eb3b9ba1e406a5aa68001a66cb82bac95748839dc02dd10048472c1")
set(_LUA_DEST    "${CMAKE_BINARY_DIR}/_vendored")
set(_LUA_SRC     "${_LUA_DEST}/lua-5.1/src")

if(NOT EXISTS "${_LUA_TARBALL}")
    message(FATAL_ERROR "Vendored Lua tarball missing: ${_LUA_TARBALL}")
endif()

# Pin the source: verify the tarball hash before using it.
file(SHA256 "${_LUA_TARBALL}" _got_sha)
if(NOT _got_sha STREQUAL _LUA_SHA256)
    message(FATAL_ERROR "Lua tarball hash mismatch.\n  expected ${_LUA_SHA256}\n  got      ${_got_sha}")
endif()

if(NOT EXISTS "${_LUA_SRC}/lua.h")
    file(MAKE_DIRECTORY "${_LUA_DEST}")
    file(ARCHIVE_EXTRACT INPUT "${_LUA_TARBALL}" DESTINATION "${_LUA_DEST}")
endif()
if(NOT EXISTS "${_LUA_SRC}/lua.h")
    message(FATAL_ERROR "Lua extraction failed: ${_LUA_SRC}/lua.h not found")
endif()

# Library sources only (exclude the standalone interpreter lua.c, compiler luac.c
# and the luac helper print.c).
set(_LUA_LIB_C
    lapi lauxlib lbaselib lcode ldblib ldebug ldo ldump lfunc lgc linit liolib
    llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate lstring
    lstrlib ltable ltablib ltm lundump lvm lzio)
set(_LUA_SRCS "")
foreach(c ${_LUA_LIB_C})
    list(APPEND _LUA_SRCS "${_LUA_SRC}/${c}.c")
endforeach()

add_library(lua51 STATIC ${_LUA_SRCS})
target_include_directories(lua51 SYSTEM PUBLIC "${_LUA_SRC}")
set_target_properties(lua51 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Platform configuration (matches Lua's own src/Makefile targets).
if(WIN32)
    # default luaconf: uses LoadLibrary for package.loadlib; no extra define
elseif(APPLE)
    target_compile_definitions(lua51 PRIVATE LUA_USE_MACOSX)
    target_link_libraries(lua51 PUBLIC m ${CMAKE_DL_LIBS})
else() # Linux / other *nix
    target_compile_definitions(lua51 PRIVATE LUA_USE_LINUX)
    target_link_libraries(lua51 PUBLIC m ${CMAKE_DL_LIBS})
endif()

# Lua C is old; don't let its warnings break our build.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lua51 PRIVATE -w)
elseif(MSVC)
    target_compile_options(lua51 PRIVATE /w)
endif()
