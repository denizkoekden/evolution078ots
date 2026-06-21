# Evolution 0.7.8 for Tibia 7.92


This repo contains very old, outdated engine for Tibia 7.92.
I published it because I was wasting time to looking for it. Maybe somebody will use it or datapacks will use.

Server contains all needed files to start, datapacks, map, database schema, sources to compile with dlls and libs.

I regret to work with it, repair after understand how looks like handling with network, packets sending and threading

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
# Windows: vcpkg install libxml2 boost-regex gmp sqlite3 --triplet x64-windows-static
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTORAGE=sqlite
cmake --build build --parallel
cmake --install build --prefix dist     # ready-to-run server folder
cd dist && ./evolutions                 # reads ./config.lua and ./data, listens on 7171
```

`-DSTORAGE=` selects the backend: `sqlite` (default, self-contained), `mysql`
(original 1:1, needs a server), `mysql+sqlite` (runtime choice), or `xml`
(nostalgic file-based storage). Pushing a `v*` tag builds self-contained `.zip`
packages for all three OSes via GitHub Actions (`.github/workflows/release.yml`).

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
