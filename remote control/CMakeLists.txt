###############################################################################
# Evolution Remote-Control (OTAdmin) — modern, portable CMake build
#
# The original tool was a Windows-only Win32 GUI (Dev-C++ / MinGW 3.4.2). This
# build keeps the networking / protocol / encryption core 1:1 and renders the
# UI through wxWidgets, so the same admin tool builds and runs natively on
# Windows, Linux and macOS (GCC / Clang / MinGW-w64) — the same toolchains and
# packaging approach as the modernized server.
#
#   Core (unchanged) : commands.cpp  networkmessage.cpp  rsa.cpp
#   Front-end (wx)    : gui_wx.cpp   (replaces the Win32 main/gui/stdafx + .rc)
###############################################################################
cmake_minimum_required(VERSION 3.18)

project(EvolutionRemoteControl
    VERSION 0.7.7
    DESCRIPTION "Evolution Remote-Control (OTAdmin), cross-platform wxWidgets build"
    LANGUAGES CXX)

# Match the server: GNU C++14 (keeps the GNU extensions the old code relies on).
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Reuse the server's FindGMP.cmake (one directory up).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# Help local (Homebrew) discovery on macOS.
if(APPLE)
    foreach(_p gmp wxwidgets)
        execute_process(COMMAND brew --prefix ${_p}
            OUTPUT_VARIABLE _bp OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_bp AND EXISTS "${_bp}")
            list(APPEND CMAKE_PREFIX_PATH "${_bp}")
        endif()
    endforeach()
endif()

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source")
set(SOURCES
    "${SRC_DIR}/gui_wx.cpp"          # cross-platform wxWidgets front-end
    "${SRC_DIR}/commands.cpp"        # admin command / protocol core (unchanged)
    "${SRC_DIR}/networkmessage.cpp"  # wire format + XTEA/RSA glue (unchanged)
    "${SRC_DIR}/rsa.cpp")            # GMP RSA (unchanged)

# WIN32 -> GUI subsystem on Windows (wxWidgets supplies WinMain); ignored elsewhere.
add_executable(remote-control WIN32 ${SOURCES})
set_target_properties(remote-control PROPERTIES OUTPUT_NAME "Remote-Control")
target_include_directories(remote-control PRIVATE "${SRC_DIR}")

###############################################################################
# Dependencies
###############################################################################
# --- wxWidgets (GUI) — native widgets per platform (MSW / GTK / Cocoa) -------
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})
target_link_libraries(remote-control PRIVATE ${wxWidgets_LIBRARIES})

# --- GMP (rsa.cpp) ----------------------------------------------------------
find_package(GMP REQUIRED)
target_link_libraries(remote-control PRIVATE GMP::GMP)

# --- Platform bits ----------------------------------------------------------
if(WIN32)
    # winsock2 (definitions.h includes it before <windows.h>); NOMINMAX so the
    # windows.h min/max macros don't clash with std::min in networkmessage.cpp.
    target_compile_definitions(remote-control PRIVATE
        __WINDOWS__ WIN32 _WIN32 WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(remote-control PRIVATE ws2_32)
endif()

# 1:1 wire behaviour: the message buffers are type-punned (GetU16/32, XTEA),
# exactly as in the server. Disable strict-aliasing so codegen matches the
# original GCC 3.4 build.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(remote-control PRIVATE -fno-strict-aliasing)
endif()

###############################################################################
# Install / packaging (binary + its banner/icon next to it)
###############################################################################
install(TARGETS remote-control RUNTIME DESTINATION .)
# Normalize the banner name to lowercase so the runtime load (logo.bmp) also
# works on case-sensitive filesystems (Linux).
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/logo.BMP" DESTINATION . RENAME "logo.bmp")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/otserv.ico" DESTINATION .)
