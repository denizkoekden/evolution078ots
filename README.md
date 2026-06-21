# Evolution 0.7.8 for Tibia 7.92


This repo contains a very old, outdated engine for Tibia 7.92.

I have a lot of good memories from my youth. I always loved the Evolutions
server and never quite warmed up to TFS (The Forgotten Server). I don't know
why, it's clearly the better engine. Kids…

Anyway, I forked this archive from [lambdaforg/evolution078ots](https://github.com/lambdaforg/evolution078ots) and
used modern technology to bring the nostalgia back to life. I threw some coins
at **Claude Opus 4.8** and let it fix the toolchain, so you can now enjoy the
old Evolutions server on modern platforms.

Thanks to my clanker. 😅

---

## Modernized build (Windows / Linux / macOS)

The original sources only built with Dev-C++ / MinGW (Windows). They now build
unchanged-in-behaviour with modern GCC / Clang / MSVC via **CMake**, with Lua 5.1
vendored from this repo. See **[MODERNIZATION.md](MODERNIZATION.md)** for the full
list of changes, assumptions, and the fixed pre-existing bugs.

### Quick start

```bash
# Linux  : sudo apt install cmake g++ libxml2-dev libboost-regex-dev libgmp-dev libsqlite3-dev
# macOS  : brew install cmake boost gmp libxml2 sqlite
# Windows: MSYS2/MinGW64 -> pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,libxml2,boost,gmp,sqlite3}
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTORAGE=sqlite
cmake --build build --parallel
cmake --install build --prefix dist     # ready-to-run server folder
cd dist && ./evolutions                 # reads ./config.lua and ./data, listens on 7171
```

`-DSTORAGE=` selects the backend: `sqlite` (default, self-contained), `mysql`
(original 1:1, needs a server), `mysql+sqlite` (runtime choice), or `xml`
(nostalgic file-based storage). Pushing a `v*` tag builds self-contained `.zip`
packages for all three OSes via GitHub Actions (`.github/workflows/release.yml`).

### Remote-Control (admin GUI)

The bundled admin tool in `remote control/` (originally Windows-only Win32) was
modernized to **wxWidgets** so it builds and runs natively on Windows, Linux and
macOS too, with the admin protocol and all commands kept 1:1. It builds the same
way and ships as its own per-OS zip (CI job `remote-control`):

```bash
# Linux  : sudo apt install cmake g++ libgmp-dev libwxgtk3.2-dev
# macOS  : brew install cmake gmp wxwidgets
# Windows: MSYS2/MinGW64 -> pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,gmp,wxwidgets3.2-msw}
cmake -S "remote control" -B build-rc -DCMAKE_BUILD_TYPE=Release
cmake --build build-rc --parallel
```

See **[MODERNIZATION.md](MODERNIZATION.md) §7** for the details.

> The original engine has several known crash bugs listed below; they are
> **behaviour-preserved** by the modernization (not silently changed). Two crash
> bugs that the old MSVC runtime hid by luck were fixed because they are
> deterministic on modern toolchains (see MODERNIZATION.md §4).


# Crash, bugs

List of bugs/crash if you wanna repair or disabled to stable use engine


* Party system crash when two players spamming each other join/ leave party, logout and then repeat spamming
* Trade system crash when players spamming trade, especially when one of the players doesn't have size to obtain item
* Houses might crash - commands aleta sio, aleta som, aleta grav etc (xxx*, * regex)
* Parcel system might crash if somebody put to much items
* Depot same as above, too much items put by selling house, buying reapating it with milions of items for sure will lags your server, then might crash
* Don't use account manager in tibia, after save account change to 0 from 111111  if somebody log in then crash
* If house doesn't have house-exit then might be crash (somebody invite player to house, logout, other player sell house, when you log in to first player then crash)
* Varkhal + Johnny, Johnny + Djinn/Cyclops (addon items ) Varkhal + Djinn/Cyclops those npcs can crash your server when you say something like hi>addon and on MC with another npc you say hi>yes
* Players crash server to putting to much items on tile, on door tile and close etc

All of those bugs is related for only engine (source)

Two of them, and mostly engine bugs and crash caused by bad network handling with client. If you wanna use it, you need it a lot of ram, either player can lag your server simply exit from server and then log in other character with players and keep spamming spells (player has to have fire or something to be in server)

Server propably has race condition
