# Modernization of OpenTibia Server "Evolution 0.7.8"

This document records how the original 2007-era **Evolution 0.7.8** sources (a
Dev-C++ / MinGW-GCC-3.4.2, Windows-only OTServ fork) were made to build with
modern compilers (GCC 13+, Clang 16+, MSVC 2022) on **Windows, Linux and macOS**,
**without changing runtime behaviour**.

The guiding rule for every change below: *preserve the original behaviour 1:1;
only touch what a modern toolchain rejects, and isolate anything that could
affect observable behaviour.*

---

## 1. New, portable build system (CMake)

The original authoritative build was `source/dev-C++/Makefile.win` (the Dev-C++
project — exactly **68 `.cpp`** files, `-D__USE_MYSQL__`). The bundled autotools
files (`Makefile.am`, `configure.in`, `Makefile.in`, `configure`) are **stale and
broken for this fork** (they reference files that do not exist, e.g. `md5.cpp`,
`const78.h`) and are intentionally **not used**.

New files:

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Portable build; source manifest mirrors `Makefile.win`; all original `-D` feature flags preserved verbatim. |
| `cmake/BuildVendoredLua.cmake` | Builds **PUC Lua 5.1** as a static lib from the repo's own `source/libraries/lua-5.1.tar.gz` (hash-pinned) — identical Lua on every OS. |
| `cmake/FindGMP.cmake` | Locates GNU MP. |
| `cmake/FindMySQLClient.cmake` | Locates the MySQL/MariaDB client (optional). |
| `.github/workflows/release.yml` | CI matrix → self-contained per-OS `.zip` packages. |

### Storage backend is a compile-time option

The original selected the storage layer at **compile time** (`USE_SQL_ENGINE`
picks the SQL IO classes, otherwise the XML IO classes). This is exposed as:

```
cmake -S . -B build -DSTORAGE=sqlite        # default — self-contained "download & run"
cmake -S . -B build -DSTORAGE=mysql         # original 1:1 backend (needs a MySQL server)
cmake -S . -B build -DSTORAGE=mysql+sqlite  # one binary, runtime choice via config.lua
cmake -S . -B build -DSTORAGE=xml           # nostalgic file-based storage (no SQL at all)
```

`STORAGE=sqlite` compiles the exact same SQL IO units as the original plus
`databasesqlite.cpp`; `STORAGE=mysql` reproduces the original `Makefile.win`
manifest exactly. `STORAGE=xml` is mutually exclusive with SQL (an `#else`
branch), as in the original.

### Dependencies

libxml2, Boost (regex compiled; bind/function/tokenizer/pool header-only), GMP,
Lua 5.1 (vendored), plus SQLite3 and/or the MySQL client depending on `STORAGE`.
**Boost is kept** (not replaced by `std::`): `boost::regex` is not a 1:1 swap for
`std::regex`, and `boost::function` is required by `MovePlayerTask`. **Lua 5.1**
is kept (not ported to 5.4, which would change number/string semantics).

---

## 2. How to build

```bash
# Linux  : apt install cmake g++ libxml2-dev libboost-regex-dev libgmp-dev libsqlite3-dev
# macOS  : brew install cmake boost gmp libxml2 sqlite
# Windows: MSYS2/MinGW64 -> pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,libxml2,boost,gmp,sqlite3}
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTORAGE=sqlite
cmake --build build --parallel
cmake --install build --prefix dist     # dist/ is a ready-to-run server folder
```

Run from the staged folder: `./dist/evolutions` (it reads `./config.lua` and `./data`).

---

## 3. Source changes (all behaviour-preserving)

Every change is also commented inline at the call site. Categories:

### 3.1 Removed / replaced compiler-removed constructs
- **Hash containers** (`definitions.h`): `__gnu_cxx::hash_map/hash_set` and MSVC
  `stdext::hash_map` → `std::unordered_map/std::unordered_set` (same average-O(1)
  semantics; no call site depends on iteration order).
- **Fixed-width types** (`definitions.h`, `enums.h`, `tools.h`, `game.*`):
  hand-rolled `typedef __int64 int64_t; ...` and `__int64` → `<cstdint>` /
  `int64_t` (identical widths).
- **Missing transitive includes**: `<cstdint>`, `<cassert>`, `<chrono>`,
  `<cstdlib>` added where the old toolchain leaked them implicitly.
- **MSVC/MinGW-only integer<->string funcs**: `itoa/ltoa/_ultoa/_i64toa`
  (`tools.cpp`, `game.cpp`, `commands.cpp`, `guilds.cpp`) → `snprintf` base-10;
  `_atoi64` (`monsters.cpp`, `ioplayerxml.cpp`) → `strtoll`. Byte-identical output.
- **`std::mem_fun`** stays available under gnu++14 (not removed).
- **`status.cpp`**: `"lit"MACRO` adjacent-token error → spaces added (same string).
- **`combat.cpp`**: local `round(float)` renamed `combatRound` (clashed with C99
  `::round`); its half-**down** rounding is preserved (not replaced by std::round).

### 3.2 `char*` ← string-literal (ISO C++ hard error on MSVC/GCC)
- RSA key in `otserv.cpp` and `RSA::setKey` → `const char*`.
- All `getSourceDescription()` overrides (`ioaccount*.h`, `ioplayer*.h`,
  `iomap*.h`) → `const char*`.
- `s_defcommands::name` (`commands.h`), `Logger::logMessage(channel)` and
  `type_str` (`logger.*`) → `const char*`.

### 3.3 Platform layer
- **Sockets** (`otsystem.h`, `database.h`, `databasemysql.h`, `databasesqlite.h`):
  `<winsock.h>` → `<winsock2.h>` + `<ws2tcpip.h>` (included before `<windows.h>`,
  `WIN32_LEAN_AND_MEAN`); `otserv.cpp` `WSAStartup` 1.1 → 2.2; link `ws2_32`.
  The existing `OTSYS_*` macro abstraction is kept — no socket rewrite.
- **Clock** (`otsystem.h`, `tools.cpp`): `_ftime`/`ftime`/`struct timeb`
  (removed in modern glibc / deprecated on macOS) → `std::chrono` returning the
  same milliseconds-since-epoch / elapsed-seconds value.
- **`exception.cpp`**: the Win32 SEH stack-tracer was guarded only by `__GNUC__`
  (true on Linux/macOS clang/gcc too). It is now also guarded by `WIN32`, so on
  non-Windows `dumpStack()` is a no-op — exactly the original effect (the tracer
  is only ever used under `__EXCEPTION_TRACER__`, which this build does not define).
- **`__NO_HOMEDIR_CONF__`** defined for all platforms → config is read from
  `./config.lua` everywhere (see Assumptions).

### 3.4 Dead code that no longer parses
- `game.h`: the never-instantiated `TCallList<>` template and its 5-arg
  `makeTask<>` overload were removed. Modern compilers eagerly type-check the
  template body, which calls `Task::operator()` on the incomplete type `Task`
  (a `game.h`↔`tasks.h` include cycle). The original never instantiated them, so
  this code never actually compiled; removing unreachable code keeps behaviour 1:1.

---

## 4. Pre-existing bugs found and fixed (needed to build/run on a modern toolchain)

1. **`tools.cpp` `upchar()` data corruption** — the accented-letter upper-casing
   table had every CP1252 high byte corrupted to U+FFFD (the Unicode replacement
   char) somewhere in the archive's history, collapsing all 31 branches to one
   value. Reconstructed as the standard Latin-1/CP1252 mapping
   (à..þ → À..Þ via `-0x20`, ÿ → Ÿ). **Assumption** — see below.
2. **`game.cpp` `listOnline()` crash** — dereferenced `listPlayer.list.begin()`
   without an `end()` check; with 0 players online (startup) this read past a
   global array (confirmed via AddressSanitizer). "Worked" by luck on the old
   MSVC STL. Fixed with the missing `end()` guard.
3. **`databasesqlite.h`** — `class DatabaseSqLite : protected _Database` (typo;
   `DatabaseMySQL` uses `public`). The SQLite path was never compiled in the
   original MySQL-only build, hiding it. Fixed to `public`.
4. **`database.h` `getText()`** returned `c_str()` of a destroyed temporary
   (dangling). Cached in a member.
5. **`database.cpp` `DBResult::clear()`** — `delete[]` on scalar `new` (`RowData`,
   `unsigned long`) → `delete`.
6. **`ban.cpp` / `game.cpp`** — `return false;` from functions returning
   `std::string` / `Item*` (relied on the pre-C++11 "false is a null pointer
   constant" rule) → `return "";` / `return NULL;`.

---

## 5. Stated assumptions

- **The original was Windows/Dev-C++ only.** The autotools build is treated as
  dead and ignored; `Makefile.win` is the source of truth for the file manifest
  and feature flags.
- **Config path**: the original `*nix` code read `$HOME/.otserv/config.lua`. Since
  there was never a working `*nix` build, the packages define `__NO_HOMEDIR_CONF__`
  and read `./config.lua` on every OS so the zips are "download & run".
- **`upchar()` reconstruction** restores the *original Evolution* behaviour, which
  is more faithful than preserving the corrupted (and uncompilable) archive state.
  If an authentic, uncorrupted Evolution 0.7.8 `tools.cpp` is available, the table
  can be diffed against it.
- **Default published package = SQLite** (self-contained). MySQL and XML are
  also built and shipped (MySQL needs an external server).

---

## 6. CI / Releases

`.github/workflows/release.yml` builds a matrix of
`{windows, linux, macos} × {sqlite, xml, mysql}` and bundles all runtime
dependencies into self-contained `.zip` packages (Windows: MinGW DLLs bundled
next to the exe; macOS: `dylibbundler`; Linux: bundled `.so` + `$ORIGIN/lib`
RPATH). Pushing a tag `v*` publishes the zips as a GitHub Release.

**Windows uses MinGW-w64 (MSYS2), not MSVC/vcpkg** — it is the modern successor of
the original Dev-C++ MinGW toolchain, its prebuilt pacman packages install in
seconds (vcpkg rebuilt Boost/libxml2/GMP from source on every run), and it shares
libstdc++ with the Linux build, so the two behave identically. The code remains
MSVC-compatible (winsock2, `const char*`, `_MSC_VER` shims), but CI ships MinGW.

---

## 7. Remote-Control (OTAdmin) — cross-platform admin GUI

The bundled admin tool under `remote control/` was a **Windows-only Win32 GUI**
(Dev-C++ / MinGW 3.4.2: `WinMain`, `CreateWindowEx`, `WndProc`, `.rc` resources).
A Win32 GUI cannot be *both* byte-identical *and* run natively on Linux/macOS, so
the split here is deliberate:

- **Core kept 1:1** — `commands.cpp` (the 27 admin commands + login/encryption
  handshake), `networkmessage.cpp` (wire format + XTEA/RSA), `rsa.cpp` (GMP). These
  already shipped POSIX socket branches; they only needed the Win32 GUI headers
  removed and a few transitive includes added (`<algorithm>`, `<cstring>`,
  `<cctype>`) — the same class of edit as the server. The admin **protocol is
  unchanged**, so it talks to the exact same server.
- **Front-end re-implemented in wxWidgets** — `source/gui_wx.cpp` replaces the
  Win32 `main.cpp` + `gui.cpp` + `stdafx.cpp` + the `.rc` resources. wxWidgets uses
  the platform's **native** widgets (MSW / GTK / Cocoa), so on Windows it looks
  essentially like the original. The menu tree, the input prompts and every command
  are reproduced 1:1; the old `CInputBox` modal becomes `wxTextEntryDialog`, and the
  core's `addConsoleMessage()` hook (now `frontend.h`) routes into the console pane.

| File | Purpose |
|------|---------|
| `remote control/CMakeLists.txt` | Standalone portable build (wxWidgets `core base` + GMP + sockets). `-fno-strict-aliasing` for the type-punned message buffers, like the server. |
| `remote control/source/frontend.h` | Portable logging hook the core calls; implemented by the active front-end. |
| `remote control/source/gui_wx.cpp` | The wxWidgets front-end. |

Decoupling edits to the core: `definitions.h` now pulls in winsock2 before
`<windows.h>` (wx-compatible, links `ws2_32`) and defines a POSIX
`WSAGetLastError()`→`errno` shim; `commands.cpp` / `networkmessage.cpp` drop the
Win32 GUI includes (`stdafx.h`, `gui.h`, `resource.h`) for `frontend.h`. The dead
`extern HWND hStatusWindow;` (only referenced from already-commented code) was
removed. No command logic changed.

Build it like the server:

```bash
# Linux  : apt install cmake g++ libgmp-dev libwxgtk3.2-dev
# macOS  : brew install cmake gmp wxwidgets
# Windows: MSYS2/MinGW64 -> pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,gmp,wxwidgets3.2-msw}
cmake -S "remote control" -B build-rc -DCMAKE_BUILD_TYPE=Release
cmake --build build-rc --parallel
cmake --install build-rc --prefix dist-rc   # Remote-Control + logo.bmp next to it
```

CI builds it for all three OSes in the `remote-control` job of
`release.yml` and ships per-OS zips alongside the server packages (Linux bundles
wxWidgets+GMP and relies on the system GTK3; macOS uses `dylibbundler`; Windows
bundles the MinGW DLLs). The app version string is kept at the original **0.7.7**.
