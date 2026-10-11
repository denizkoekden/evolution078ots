# Crash fixes (bugfix variant)

Long-standing engine crashes from the original Evolution 0.7.8, fixed on the
`bugfixes` branch and shipped as the **`v0.7.9-bugfix` pre-release** (the clean,
1:1-modernized engine remains the `v0.7.8` "Latest" release / `master`).

Each fix is the smallest change that stops the crash. One of them changes a game
mechanic: with #7, a system move into a full depot (house transfer, parcel
delivery) is rejected instead of growing the depot past its limit. Root causes
were located by static analysis of the current sources (the regex match-time throw
was reproduced offline against the project's Boost).

| # | Crash | Root cause | Fix | File |
|---|-------|-----------|-----|------|
| 1 | House with no exit → crash when an uninvited player logs in on a house tile | `HouseTile::__queryDestination` returns a NULL cylinder (the entry tile doesn't exist); dereferenced on login | NULL-guard, fall through to the default destination (the tile itself) | `housetile.cpp` |
| 2 | `aleta sio/som/grav` guest-list patterns crash | `boost::regex_match` throws `regex_error` at match time on a pathological pattern; uncaught | `try/catch` around the match (skip the bad pattern) | `house.cpp` |
| 3 | Parcel with too many / malformed items crash | `Mailbox::getReceiver` dereferences a NULL `Container*` | NULL-guard before reading the label | `mailbox.cpp` |
| 4 | Account manager: account becomes 0 → crash on login | `loadPlayer()`'s `false` return is discarded; a half-initialized Player is logged in | check the return, reject the login (Account Manager stays exempt) | `otserv.cpp` |
| 5 | Party join/leave/logout spam crash | the 5 party packet handlers mutate party state **without** `g_game.gameLock` (every other handler takes it) → use-after-free vs the dispatcher thread | take the game lock in all 5 handlers | `protocol79.cpp` |
| 6 | Trade spam (esp. no inventory room) crash | `Game::playerAcceptTrade` dereferences a NULL `tradeItem` when a side reaches `TRADE_ACCEPT` without offering an item | require both trade items non-NULL before completing | `game.cpp` |
| 7 | Depot overflow crash | system moves use `FLAG_NOLIMIT`, bypassing `maxDepotLimit`; the depot grows unbounded and OOMs on save | enforce the hard cap even for system moves (rejected move keeps the item in its source) | `depot.cpp` |
| 8 | Stacking >10 items on a tile then turning → client crash/desync | `sendCreatureTurn` sends a turn-update for stack slot ≥10 (the 7.x client can't address it); it is the only tile-send path missing the guard | add `stackPos < 10` (mirrors the sibling send paths) | `protocol79.cpp` |
| 9 | Addon/promote NPC cross-talk crash on multi-client | `talk_state` is a global in a shared NPC Lua state, leaking across conversations | declare `local talk_state` (matches `guild.lua`) | `data/npc/scripts/addon.lua`, `promote.lua` |
| 10 | Online-list thread crash during logins/logouts | `listOnline` walked `Player::listPlayer` every 2 minutes without `gameLock` while other threads modify the map | hold `gameLock` while building the list (file write and sleep stay outside) | `game.cpp` |
| 11 | Bursts of connections: some never answered, some handled twice | the accept loop passed `&s` (a loop variable) to the new thread, which read it only after the next `accept()` had overwritten it; 300 parallel status queries got 215–219 answers | each thread gets its own heap copy of the socket (300/300 answered) | `otserv.cpp` |

**Notes / honesty:**
- #8 and #9 stop the crash *premise* by static reasoning, but their full
  crash-elimination was not confirmed against a live 7.x client + the exact
  spam/multi-client sequence (we have no client here). They are low-risk and follow
  established patterns in the codebase.
- A broader safety net was deliberately **not** added: the dispatcher task wrapper
  (`tasks.h`) has no `try/catch`, which is why several of the above became full
  server crashes. Wrapping it would catch many crash classes at once but can mask
  inconsistent state, so it is left out of these point fixes.
- The party lifetime guard (cleanup inside `~Player`) was also **not** added: the
  existing logout path (`game.cpp`) already removes a player from parties under the
  lock, and mutating party state from a destructor risks new dangling-pointer
  crashes. Fix #5 (locking) addresses the documented race.
