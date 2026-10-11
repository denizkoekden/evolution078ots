# Review-Notizen: Modernisierung Evolution 0.7.8

**Stand:** 2026-10-11 · **geprüft:** `master` @ `c954f15` (dazu ein Blick auf den Branch `bugfixes` @ `3ccee10`)

**Methode:** Ich habe nur Code, Build-Dateien, Skripte, Daten und Commit-Messages angesehen. **Keine `.md`-Datei
gelesen** (Vorgabe), auch nicht die, auf die Commit-Messages verweisen (`MODERNIZATION.md`, `CRASHFIXES.md`,
`DEPLOYMENT.md`, `README.md`). Am Code ist nichts geändert, diese Datei ist die einzige Änderung im Repo.
Der Abgleich mit den `.md`-Dateien folgt in Abschnitt 9 (Phase 2).

> Zeilennummern beziehen sich auf `master` @ `c954f15`. Auf dem Fix-Branch `fix/review-findings` verschieben sie
> sich.

**Legende:**

- Nachweis: **[verifiziert]** = per Build, Test, Disassembly oder DB-Abfrage bestätigt · **[Code]** = aus der
  Code-Lektüre, nicht ausgeführt
- Herkunft: *Modernisierung* = neu eingeführt · *Altlast* = schon 2007 im Code, durch die Modernisierung aber
  relevant geworden

---

## 1. Gesamturteil

Die Portierung von Build und Compiler ist handwerklich ordentlich. Die Diffs sind klein und fast jede Änderung ist
im Code begründet. Als Fundament ist das brauchbar.

**Für den Betrieb ist es aber noch nicht freigabefähig.** Vier Muster ziehen sich durch:

1. **Verifiziert wurde nur bis „kompiliert und startet“.** Die Laufzeitpfade wurden unter den neuen Standard-Setups
   (SQLite, MariaDB 11, GCC 13) nicht getestet: Login, Speichern, Charaktererstellung, GM-Befehle. Genau dort
   liegen die schwersten Befunde (K3, K4).
2. **Compiler-Warnungen wurden ignoriert.** GCC meldet die gefährlichsten Stellen sogar ohne Zusatzflags
   (`-Wreturn-type`). Eine davon führt in Linux-Builds nachweislich zum Absturz (K3).
3. **Das Deployment macht den Server internettauglich, ohne die Altlasten zu beheben, die dadurch erst gefährlich
   werden.** Dazu gehören der Standard-GM-Account, das SQLite-Escaping und das fehlende geordnete Herunterfahren.
4. **Einige Aussagen in Kommentaren und Commit-Messages stimmen nicht oder sind übertrieben**, etwa „1:1“,
   „builds clean“ und „verified end-to-end“. Siehe Abschnitt 7.

---

## 2. Was gut gemacht ist

- **Kleine Diffs mit Begründung im Code.** Man kann jede Änderung nachvollziehen.
- **CMake:** Die `STORAGE`-Option ist sauber umgesetzt. Der Lua-Tarball ist per SHA256 gepinnt, und die Quellliste
  entspricht `Makefile.win`.
- **Echte Bugfixes:**
  - `Ban::getReason` gibt `""` statt `return false` zurück; Letzteres war ein `std::string(NULL)` und damit UB.
  - `findItemOfType` gibt `NULL` statt `false` zurück.
  - Der Dangling Pointer in `DBQuery::getText()` ist behoben.
  - Der `delete[]`/`delete`-Mismatch ist behoben.
  - `DatabaseSqLite` erbt jetzt `public` statt `protected`.
  - Der Iterator im Account-Manager ist korrigiert (`c954f15`).
  - `listOnline` hat jetzt einen `end()`-Guard.
- **`combatRound` statt `round`:** Die Half-down-Semantik des Originals ist bewusst und korrekt erhalten.
- **Plattform-Details:**
  - Die Include-Reihenfolge `winsock2` vor `windows.h` stimmt.
  - Im Server gibt es `OTSYS_EWOULDBLOCK`, statt `EWOULDBLOCK` umzudefinieren.
  - Attribut-Slots nutzen `intptr_t`.
  - Der SEH-Tracer ist auf `__i386__` beschränkt.
- **`std::unordered_map` statt `hash_map`:** Laut Kommentar hängt keine Aufrufstelle von der Reihenfolge ab. Das
  stimmt: Beide Iterationsstellen nutzen `find()` ([ioplayersql.cpp:225](source/ioplayersql.cpp#L225),
  [iomapserializesql.cpp:298](source/iomapserializesql.cpp#L298)).
- **Case-Fixes in `data/`:** Alle 966 `script=`/`file=`-Verweise und alle `dofile`-Pfade lösen mit exakter Groß-
  und Kleinschreibung auf. Dasselbe gilt für die Welt-Dateien aus `config.lua` und die Dateinamen, die in der
  Karte stehen (`Evolutions-spawn.xml`, `Evolutions-house.xml`). **[verifiziert]**
- **`source/db.s3db`** passt exakt zum DDL in `database.sqlite.sql`. **[verifiziert]**
- **Remote-Control-Port:**
  - Der Menübaum entspricht 1:1 dem Original-`otserv.rc`, inklusive des dort auskommentierten
    „Get Online Players“. Die leere Spielerliste und die leere „Status Messages“-Liste sind also originalgetreu.
  - Nebenbei ist ein Original-Bug behoben: `WSAStartup`/`WSACleanup` lief bei *jeder* Window-Message.
- **Docker:** Multi-Stage-Build, Non-Root-User und ein Healthcheck über die echte Status-Abfrage statt eines
  bloßen Port-Checks. Gute Idee.

---

## 3. Befunde nach Schwere

### Kritisch

#### K1 – SQLite-Backend (Standard!): kaputtes Escaping, Saves scheitern, Daten werden verfälscht · [verifiziert per Unit-Test] · Altlast, durch „SQLite als Standard“ aktiviert

> **Korrektur nach Test:** In der ersten Fassung stand hier „SQL-Injection wahrscheinlich“. Der Test zeigt etwas
> anderes. `escape_string()` setzt das Escape-Zeichen *hinter* das Zeichen: Aus `'` wird `'\`, aus einem
> Zeilenumbruch `n\`. In SQLite folgt auf jedes eingeschleuste `'` also ein `\`, und das ist immer ein
> Syntaxfehler. Eine Injection ist im reinen SQLite-Build damit praktisch ausgeschlossen. Die tatsächliche Folge
> ist **Datenverlust**.

- **Escaping:** [source/database.cpp:266](source/database.cpp#L266) `escape_string()` passt nicht zu SQLite.
  Korrekt wäre, `'` zu `''` zu verdoppeln.
- **Ausführung:** [source/databasesqlite.cpp:82](source/databasesqlite.cpp#L82) und
  [:106](source/databasesqlite.cpp#L106) nutzen `sqlite3_exec`. Das führt mehrere `;`-getrennte Statements aus.
- **Betroffene Strings:**
  - Item-Texte, also Briefe und Bücher ([ioplayersql.cpp:429](source/ioplayersql.cpp#L429) ff.)
  - Haus-Zugangslisten ([iomapserializesql.cpp:415](source/iomapserializesql.cpp#L415) ff.)
  - Report-Texte ([ioplayersql.cpp:729](source/ioplayersql.cpp#L729))
- **Folge 1:** Ein einziger Apostroph genügt (ein Brief mit „Don't…“). Das INSERT wird zum Syntaxfehler,
  `savePlayer` bricht ab, und SQLite rollt den **kompletten Save** zurück. Dieser Spieler wird nie wieder
  gespeichert, solange er das Item trägt. Für Häuser gilt dasselbe.
- **Folge 2:** Zeilenumbrüche werden zu `n\` verfälscht, und Backslashes verdoppeln sich bei jedem Save.
- **Folge 3:** Der Blob-Pfad schneidet Binärdaten am ersten Null-Byte ab (`std::string(s)`) und liest per `strlen`.
  Haus-Item-Attribute werden dadurch beschädigt.
- **Injection** ist real nur im Build `mysql+sqlite` mit SQLite zur Laufzeit. Dort wird `mysql_escape_string`
  benutzt, und `'` *beendet* in SQLite das Literal (M5).
- **Hintergrund:** Laut dem Kommentar der Modernisierung in `databasesqlite.h` wurde der SQLite-Pfad im Original nie
  kompiliert. Die Modernisierung hat ihn zum Standard gemacht, ohne ihn zu prüfen.
- **Nachweis:** Ein Unit-Test gegen die echte DB-Schicht (Text mit `'`, `"`, `\`; Blob mit Null-Bytes) besteht auf
  dem Fix-Branch und scheitert auf `master` („unrecognized token: \“).
- **Fix (umgesetzt):**
  - `sqlite3_prepare_v2`/`sqlite3_step`, genau ein Statement pro Aufruf
  - Werte mit echter Länge
  - `''`-Quoting
  - Blobs als `X'..'`
  - Escaping zur Laufzeit je nach aktivem Backend

#### K2 – Ausgelieferter GM-Account mit bekanntem Standardpasswort · [verifiziert] · Altlast, durch das Deployment-Tooling relevant

- `source/db.s3db` und `database.sql` enthalten „GM Xidaozu“ (access 5) auf Account 222222. Das Passwort ist
  identisch mit dem des öffentlich dokumentierten Demo-Accounts 111111.
- Das XML-Backend hat denselben Seed: `data/players/GM Xidaozu.xml` (access 5) und
  `data/accounts/222222.xml` mit demselben Passwort.
- Damit hat jede frische Installation einen GM-Login mit bekanntem Passwort. Das gilt für alle drei Backends
  (SQLite, MySQL, XML) und alle Auslieferungswege (Release-Zip, Docker, Pterodactyl).
- **Verschärfend:**
  - Passwörter liegen im Klartext in der DB.
  - `logintries = 0` in [config.lua:74](config.lua#L74) schaltet die Login-Sperre komplett ab
    ([ban.cpp:52](source/ban.cpp#L52)).
- **Fix:**
  - GM-Seed nicht ausliefern, oder beim ersten Start ein Zufallspasswort erzeugen und ausgeben.
  - Den Demo-Account ohne Rechte anlegen und `logintries > 0` setzen.
  - Mittelfristig Passwort-Hashing einführen.
- **Entscheidung (User, 2026-10-11):** Der Account bleibt bewusst erhalten. Umgesetzt ist stattdessen eine
  Warnung beim Start, solange das Standardpasswort gesetzt ist (siehe §10).

#### K3 – `/save` stürzt in GCC-Builds ab (Linux, Docker, Pterodactyl) · [verifiziert per Disassembly; Absturz nicht live ausgelöst] · Altlast, durch den modernen Compiler scharf geschaltet

- [source/commands.cpp:1360](source/commands.cpp#L1360): `Commands::saveServer` hat kein `return`.
- GCC 13.3 (genau der Docker-Build) erzeugt für diese Funktion **kein einziges `ret`**. Nach `Game::saveServer()`
  läuft der Code in den Exception-Cleanup und ruft `_Unwind_Resume` mit Müll auf. Ergebnis: Prozessabbruch, wenn
  ein GM `/save` eingibt.
- Auf macOS ist es zufällig harmlos: Apple Clang erzeugt `return 0`.
- **Insgesamt 14 Stellen mit `-Wreturn-type`.** GCC meldet sie **standardmäßig** (22 Meldungen im Docker-Build):
  - [ban.cpp:297](source/ban.cpp#L297), [:314](source/ban.cpp#L314), [:326](source/ban.cpp#L326)
  - [beds.h:81-82](source/beds.h#L81)
  - [commands.cpp:1368](source/commands.cpp#L1368)
  - [container.cpp:607](source/container.cpp#L607)
  - [game.cpp:3550](source/game.cpp#L3550)
  - [mailbox.cpp:102](source/mailbox.cpp#L102)
  - [player.cpp:627](source/player.cpp#L627), [:2354](source/player.cpp#L2354)
  - [teleport.cpp:178](source/teleport.cpp#L178)
  - [tile.cpp:983](source/tile.cpp#L983)
  - [trashholder.cpp:91](source/trashholder.cpp#L91)
- **Weitere Belege aus dem GCC-Binary:**
  - `Mailbox::__removeThing`, `Teleport::__removeThing` und `TrashHolder::__removeThing` liegen auf **derselben
    Adresse** wie der statische Initialisierer `_GLOBAL__sub_I_g_beds`. Ein Aufruf würde fremden Code ausführen.
    Im normalen Spielbetrieb werden sie aber wohl nicht aufgerufen.
  - `Ban::getReason(...)` gibt `std::string` zurück, endet aber ohne `return`, wenn kein passender Ban mit
    nicht-leerem Grund existiert. Aufgerufen wird das beim Login gebannter IPs und Charaktere
    ([otserv.cpp:199](source/otserv.cpp#L199) ff.). Das ist ein Absturz-Kandidat. [Code]
  - `Container::`, `Player::` und `Tile::__removeThing` liefern auf dem Erfolgspfad einen unbestimmten
    Rückgabewert. Sie haben aber ein `ret`, ein Absturz ist dort also nicht direkt zu sehen.
- **Fix:** Explizite Returns an allen 14 Stellen. In CMake `-Wall` und `-Werror=return-type` setzen, damit das nie
  wieder durchrutscht.

#### K4 – MySQL-/Docker-Pfad: Speichern und Charaktererstellung kaputt · [verifiziert mit MariaDB 11.8.9] · Altlast + Modernisierung

**Fehlende Tabelle `blessings`:**

- `database.sql` hat keine Tabelle `blessings`, obwohl `__XID_BLESS_SYSTEM__` aktiv ist.
- `savePlayer` führt am Ende `DELETE FROM blessings` aus, bekommt **ERROR 1146** und gibt `false` zurück
  ([ioplayersql.cpp:607](source/ioplayersql.cpp#L607)). Beim Logout wird das 3-mal versucht, dann kommt
  „Error while saving player“.
- Mit MyISAM bleiben die vorherigen Statements stehen; nur die Blessings gehen verloren. **Wer auf InnoDB umstellt,
  verliert per Rollback den kompletten Save.**
- Der Fix in `c954f15` hat `blessings` nur ins SQLite-Schema übernommen, nicht in `database.sql`.

**Strict-Mode von MariaDB 11:**

- MariaDB 11 läuft standardmäßig mit `STRICT_TRANS_TABLES`.
- `players.comment` und `players.hide` sind `NOT NULL` ohne Default ([database.sql:138](database.sql#L138)).
- `createPlayer` fügt nur `(id,name,account)` ein. Ergebnis: **ERROR 1364**, der Account-Manager kann **keine
  Charaktere anlegen**.
- Dasselbe Muster gibt es bei `items.decaystate/text/specialdesc`, `commands.*`, `spells.*`, `bans.reason` und
  `guilds.guildstory`. Dort werden die Spalten im Moment meist befüllt, es bleibt aber fragil.

**Einordnung:** `cf709ad` nennt das „verified end-to-end“. Getestet wurden offenbar nur Start, Verbinden und Lauschen.

**Fix:**

- `blessings` in `database.sql` aufnehmen.
- Defaults für alle `NOT NULL`-Spalten ergänzen.
- Übergangsweise `SET SESSION sql_mode=...` nach dem Connect.

### Hoch

#### H1 – `master` und `bugfixes` sind auseinandergelaufen · [verifiziert]

- Beide Pterodactyl-Eggs bauen standardmäßig `GIT_REF=bugfixes`, beschrieben als „crash-hardened, recommended“.
- **Auf `bugfixes` fehlt `c954f15`:**
  - Es liegt das alte 27-KB-`db.s3db` ohne `blessings`.
  - Es gibt kein `database.sqlite.sql`.
  - Der Account-Manager-Fix fehlt, ebenso die wx-GUI.
  - Folge: Das SQLite-Egg installiert genau die DB, mit der laut dem master-Commit der Login scheitert.
- **Auf `master` fehlen umgekehrt die 9 Crash-Fixes** aus `5b2d774`.
- **Fix:** Einen Stand zusammenführen und die Eggs auf ein Release-Tag pinnen.

#### H2 – Kein geordnetes Herunterfahren · [Code]

- Nur `SIGPIPE` wird behandelt ([otserv.cpp:577](source/otserv.cpp#L577)). Für SIGTERM und SIGINT gibt es keinen
  Handler.
- **Docker:** `exec ./evolutions` macht den Server zu PID 1. Ohne Handler ignoriert PID 1 SIGTERM, also folgt nach
  10 s SIGKILL.
- **Pterodactyl:** Stop per `^C` (SIGINT) bricht den Prozess sofort ab.
- In beiden Fällen gibt es **keinen Save**. Verloren gehen bis zu `autosave = 10` Minuten Fortschritt, also Spieler-
  und Hausdaten.
- **Fix:**
  - Ein Signal-Handler, der Save und Shutdown auf dem Game-Thread einplant.
  - In Docker `STOPSIGNAL` und `stop_grace_period` setzen, ggf. `init: true`.

#### H3 – Reinstall und Update überschreiben Live-Daten · [Code, nicht ausgeführt]

- **Pterodactyl:** Die Egg-Installation macht `cmake --install build --prefix /mnt/server`. Bei „Reinstall“ werden
  damit `config.lua`, das ganze `data/` und `db.s3db` überschrieben. Beim SQLite-Egg ist `db.s3db` die
  Spieler-Datenbank.
- **Release-Zip:** Wer ein Update darüber entpackt, überschreibt seine `db.s3db`.
- **Fix:** DB und Config nur anlegen, wenn sie fehlen. Also ein Template wie `db.s3db.dist` oder die DB beim ersten
  Start erzeugen. Laufzeitdaten gehören nicht in den Install-Prefix.

#### H4 – Linux-Pakete sind nicht „download & run“ · [verifiziert]

- CI baut auf `ubuntu-latest` (derzeit 24.04 oder neuer). Gemessen habe ich am Docker-Build auf `ubuntu:24.04`:
  Das Binary verlangt **GLIBC_2.38**. Ein neueres `ubuntu-latest` würde es nur verschlimmern.
- Es läuft daher nicht auf Debian 12 (2.36), Ubuntu 22.04 (2.35) oder RHEL 9 (2.34).
- libstdc++ wird mitkopiert, glibc lässt sich nicht mitkopieren.
- **Fix:** In einem älteren Container bauen, etwa Ubuntu 20.04 oder Debian 11, oder libstdc++/libgcc statisch
  linken.

### Mittel

#### M1 – `upchar()` ist nicht 1:1, und der Kommentar ist sachlich falsch · [verifiziert]

- [source/tools.cpp:135](source/tools.cpp#L135): Laut Kommentar war die Original-Quelle „auf U+FFFD korrumpiert“.
  **Das stimmt nicht.** Ein Hexdump von `0db70b7` zeigt intakte Latin-1-Bytes (`E0→C0` … `FF→DF`). Nur die UTF-8-
  Anzeige des Diffs zeigte „�“.
- Die neue Formel trifft 30 der 31 Original-Paare. Bei `0xFF` weicht sie ab: Original `ÿ→0xDF`, neu `ÿ→0x9F`.
- Dieselbe Fehldiagnose bei `hexdump()` ([tools.cpp:128](source/tools.cpp#L128)): Das Original-Zeichen war `0xB7`
  („·“), jetzt steht dort `'.'`.
- **Praktische Auswirkung:** klein.
- **Lehre:** Den „1:1“-Aussagen der Modernisierung kann man nicht blind trauen. Der Kommentar führt künftige
  Maintainer in die Irre.

#### M2 – Speicherleck in `DBResult::clear()` · [Code] · Altlast, die Zeilen wurden angefasst

- [source/database.cpp:140](source/database.cpp#L140): Die Arrays `row` und `length` (`new char*[n]`,
  `new unsigned long*[n]`) werden nie freigegeben. Das leckt pro Ergebniszeile jeder Query.
- Die Modernisierung hat genau diese Zeilen geändert (`delete[]` → `delete`), das Leck aber übersehen.

#### M3 – `listOnline`-Thread ohne Lock · [Code] · Altlast, angefasst

- [source/game.cpp:283](source/game.cpp#L283) iteriert alle 2 Minuten über `Player::listPlayer`, ohne `gameLock`.
  Login und Logout ändern die Map gleichzeitig. Das ist ein Data Race und ein Absturz-Kandidat.
- Der `end()`-Guard behebt nur den Fall der leeren Liste.
- `info.erase(info.length(), …)` tut nichts.

#### M4 – Socket wird per Adresse einer Schleifenvariable übergeben · [Code] · Altlast

- [source/otserv.cpp:961](source/otserv.cpp#L961): `OTSYS_CREATE_THREAD(ConnectionHandler, (void*)&s)`.
  `s` lebt nur bis zum Ende der Schleifeniteration. Bei schnellen parallelen Verbindungen können zwei Threads
  denselben Socket bekommen.
- Außerdem `if(s > 0)`: Unter Windows ist `INVALID_SOCKET` unsigned, die Bedingung ist dort also wahr.
- **Fix:** Den Socket by value übergeben.

#### M5 – Der `mysql+sqlite`-Build hält die „Laufzeitwahl“ nicht ein · [Code]

- Die Dialekt-Weichen sind zur Compile-Zeit festgelegt (`#ifdef __USE_SQLITE__`):
  - `LIMIT 1` entfällt auch für MySQL ([ioaccountsql.cpp:153](source/ioaccountsql.cpp#L153),
    [ioplayersql.cpp:384](source/ioplayersql.cpp#L384)).
  - `createPlayer` nimmt den `NULL`-Zweig.
  - `escapeString` nutzt immer `mysql_escape_string`, auch wenn SQLite läuft.
- CI baut diese Variante nicht.
- Der `#else`-Zweig in `createPlayer` ist überflüssig: `NULL` funktioniert in beiden Datenbanken.

#### M6 – Config wird per `sed` gepatcht · [Code]

- [pterodactyl/start.sh:9](pterodactyl/start.sh#L9) escaped `|`, `&` und `\` nicht.
- Weder `start.sh` noch [docker/entrypoint.sh:14](docker/entrypoint.sh#L14) escapen `"`.
- Folge: Passwörter mit Sonderzeichen zerstören die Lua-Syntax von `config.lua` oder landen dort als Lua-Code.
- **Besser:** ENV-Overrides direkt im C++-`ConfigManager` per `getenv` lesen.
- `os.getenv` in `config.lua` geht nicht ohne Weiteres: Der Lua-State für die Config öffnet keine Bibliotheken
  ([configmanager.cpp:37](source/configmanager.cpp#L37)).

#### M7 – Lücken in der CI · [Code]

- **Kein einziger Laufzeittest.** K3 und K4 wären mit einem Smoke-Test aufgefallen: Start, Status-Abfrage,
  Login/Save gegen einen MariaDB-Service-Container.
- `brew install … || true` ([release.yml:61](.github/workflows/release.yml#L61)) verschluckt Fehler.
- `permissions: contents: write` gilt für alle Jobs statt nur für den Release-Job.
- `macos-latest` baut nur arm64; Intel-Macs bekommen kein Paket.
- Nicht geprüft: Ob dylibbundler nach `install_name_tool` neu ad-hoc signiert. arm64 verlangt eine gültige
  Signatur. [unverifiziert]
- Die Abhängigkeitslisten existieren mehrfach und laufen auseinander: CI je Betriebssystem, Dockerfile Build und
  Runtime, zwei Eggs.

#### M8 – CMake-Details · [verifiziert mit CMake 4.4.3]

- [cmake/FindMySQLClient.cmake:31](cmake/FindMySQLClient.cmake#L31): `pkg_check_modules` mit Alternativen verlangt
  *alle* Module. Der Fallback greift also nie; richtig wäre `pkg_search_module`.
- [CMakeLists.txt:199](CMakeLists.txt#L199): FindBoost erzeugt ab CMake 3.30 die Policy-Warnung CMP0167.
  Besser: `cmake_policy(SET CMP0167 NEW)` und BoostConfig.
- [CMakeLists.txt:228](CMakeLists.txt#L228): `SQLite::SQLite3` ist in CMake 4.x deprecated, der Kommentar behauptet
  das Gegenteil. Besser `SQLite3::SQLite3` mit Fallback.
- **Keine Warnflags.** Ohne `-Wall` bringt `WARNINGS_AS_ERRORS` fast nichts.
- Mit `-Wall -Wextra` (Apple Clang) gibt es 447 Warnungen. Darunter:
  - 22× `return-type` (siehe K3)
  - `string-plus-int` in [luascript.cpp:1261](source/luascript.cpp#L1261), eine Altlast:
    `"…" + info` addiert den Enum-Wert als Pointer-Offset zum String-Literal.
  - `int-in-bool-context` in `combat.cpp:566` und `creature.cpp:561`
- Der Kopf ([CMakeLists.txt:4](CMakeLists.txt#L4)) verspricht MSVC-Unterstützung. CI baut aber kein MSVC mehr.
- `-fno-strict-aliasing` verweist auf eine „memcpy remediation (step 8)“, die nie umgesetzt wurde
  ([CMakeLists.txt:178](CMakeLists.txt#L178)).

#### M9 – Lua 5.1.**0** statt 5.1.5 · [verifiziert]

- Der Tarball enthält `LUA_VERSION "Lua 5.1"`, aber kein `LUA_RELEASE`. Das ist 5.1.0 von 2006.
- 5.1.1 bis 5.1.5 sind API- und ABI-kompatible Bugfix-Releases.
- Das SHA256-Pinning ist gut, nur auf das falsche Release.

#### M10 – Remote-Control · [Code]

- [remote control/source/definitions.h:39](remote%20control/source/definitions.h#L39):
  `#define EWOULDBLOCK WSAEWOULDBLOCK`. Genau diese Umdefinition wurde im Server entfernt, weil sie modernes
  libstdc++ bricht. Hier baut es nur dank der Include-Reihenfolge.
- **Zeichensatz:**
  - [gui_wx.cpp:239](remote%20control/source/gui_wx.cpp#L239) nutzt `wxString::FromUTF8()`. Server-Texte sind
    aber Latin-1. Ungültiges UTF-8 wird zum leeren String, Meldungen mit Umlauten verschwinden also.
  - Umgekehrt schickt `ToStdString()` (Zeile 251) UTF-8 an einen Server, der Latin-1 erwartet.
- Netzwerk-I/O blockiert den GUI-Thread. Das war im Original genauso, ist in wx aber leicht zu lösen.
- Für macOS gibt es kein `.app`-Bundle.

### Niedrig

- `OTSYS_TIME` nutzt `system_clock`. Das entspricht dem Original (Wanduhr). Für den Scheduler wäre `steady_clock`
  robuster gegen Zeitsprünge; die 1:1-Entscheidung ist aber vertretbar.
- [item.cpp:907](source/item.cpp#L907) castet noch `(void*)value` statt über `intptr_t`. Das ist inkonsistent zur
  eigenen Korrektur in den Nachbarfunktionen (Zeilen 893 und 918).
- `pvparena.cpp:102` (delete auf Klasse ohne virtuellen Destruktor): hier unkritisch, weil über den eigenen Typ
  gelöscht wird.
- `mysql_escape_string` ist deprecated und ignoriert den Zeichensatz der Verbindung. Korrekt wäre
  `mysql_real_escape_string`.
- **Admin-Protokoll** ([data/admin.xml](data/admin.xml)):
  - `enabled=1` mit `onlylocalhost=1` ist gut.
  - Es nutzt aber ein Standard-Passwort und keine Verschlüsselung. Wer es für Remote-Zugriff öffnet, schickt das
    Passwort im Klartext.
- Das Runtime-Image installiert die MySQL- und die SQLite-Libs, unabhängig von `STORAGE`.
- Compose enthält feste Zugangsdaten. Für den lokalen Betrieb ist das ok; beim User-Passwort fehlt aber der Hinweis
  „ändern“.

### Branch `bugfixes` (nur überflogen)

- **Party-Locks:** sicher. Der Mutex ist rekursiv (`PTHREAD_MUTEX_RECURSIVE` bzw. `CRITICAL_SECTION`).
- **Login-Ablehnung bei einem Fehler in `loadPlayer`:** folgt dem Muster der übrigen Ablehnungszweige; ok.
- **`depot.cpp`:** Der `FLAG_NOLIMIT`-Bypass ist entfernt. Das ist eine **Mechanik-Änderung**: Paketzustellung und
  Hausräumung können jetzt am Depotlimit scheitern. Der Commit sagt dagegen „no mechanic change“. Was mit
  abgelehnten Items bei Haus-Transfers passiert, habe ich nicht geprüft.
- **`house.cpp`:** `catch(...)` um `regex_match` ist ok. Besser wäre es, die Muster schon beim Laden zu validieren.

---

## 4. Abweichungen vom Original-Verhalten (1:1-Check)

| Stelle | Änderung | Bewertung |
| --- | --- | --- |
| `upchar()` | `0xFF → 0x9F` statt `0xDF` | ungewollt, mit falscher Begründung (M1) |
| `hexdump()` | `0xB7` → `'.'` | ungewollt, harmlos (nur Debug) |
| Winsock | 2.2 statt 1.1 | nötig, ok |
| `__NO_HOMEDIR_CONF__` | `config.lua` aus dem CWD statt `$HOME/.otserv` (*nix) | bewusste, dokumentierte Annahme; ok |
| `sql_port` | neue Config-Option, Default 3306 | Erweiterung, ok |
| `listOnline` | Guard für leere Liste | Fix, ok |
| Case-Fixes in `data/` | Dateinamen für case-sensitive Dateisysteme | nötig, vollständig (verifiziert) |
| `OTSYS_TIME` / `timer()` | `chrono` statt `ftime` | gleiche Semantik, ok |

---

## 5. Bessere Wege

### Stufe 1: klein, hoher Nutzen

Diese Punkte bleiben innerhalb der bisherigen Entscheidungen (CMake, SQLite als Standard, Boost, Lua 5.1).

1. **Compiler-Warnungen als Gate:** `-Wall -Wextra -Werror=return-type` setzen und die 14 return-type-Stellen fixen
   (K3).
2. **SQL-Schicht reparieren:**
   - Prepared Statements für SQLite, mindestens korrektes Quoting und `sqlite3_prepare_v2` (K1).
   - `blessings` und Defaults in `database.sql` (K4).
3. **Keine Standard-Geheimnisse:** keinen GM-Seed ausliefern; Passwort beim ersten Start generieren; `logintries > 0`;
   Passwort-Hashing (K2).
4. **Geordnetes Herunterfahren** bei SIGTERM und SIGINT mit Save (H2).
5. **Ein Branch, ein Stand:** `bugfixes` und `master` zusammenführen; Eggs auf Release-Tags pinnen (H1).
6. **`db.s3db` nicht als Binärdatei pflegen.** Stattdessen beim Build/Install oder beim ersten Serverstart aus
   `database.sqlite.sql` + Seed-SQL erzeugen. Das verhindert genau die Drift, die auf `bugfixes` passiert ist.
   Zusammen mit „nur anlegen, wenn sie fehlt“ ist auch H3 erledigt.
7. **Pterodactyl:** das CI-Release-Zip herunterladen, statt im Installer zu kompilieren (spart Toolchain und
   Minuten). Vorhandene `config.lua`, `data/` und DB nicht überschreiben.
8. **Konfiguration per Umgebungsvariablen im `ConfigManager`** statt per `sed` (M6).
9. **CI ausbauen:**
   - Smoke-Test: Server starten, Status-Abfrage, Login/Save gegen einen MariaDB-Service-Container; Prozess per PID
     beenden.
   - Linux auf älterer glibc bauen (H4).
   - `|| true` entfernen; Rechte minimal halten.
   - `mysql+sqlite` entweder bauen und testen oder die Option streichen.
10. **CMake-Kleinigkeiten:** `pkg_search_module`, CMP0167, `SQLite3::SQLite3`; Lua auf **5.1.5** mit gleichem
    SHA-Pinning.
11. **Altlasten im angefassten Code:** DBResult-Leck (M2), Lock für `listOnline` (M3), Socket by value (M4).

### Stufe 2: größer, optional

- Type-Punning in `NetworkMessage` durch `memcpy`-basierte Zugriffe ersetzen. Danach kann `-fno-strict-aliasing`
  weg; das ist der nie gemachte „step 8“.
- Thread pro Verbindung durch eine Event-Loop ersetzen, z. B. Boost.Asio (bleibt im Boost-Ökosystem). Nur, wenn Last
  oder Robustheit es erfordern.
- `boost::bind`, `function` und `regex` durch `std::` ersetzen (C++11). Nicht nötig, reduziert aber Abhängigkeiten.
- Sanitizer-Job (ASan/UBSan) in der CI, kombiniert mit dem Smoke-Test.

---

## 6. Empirische Checks (Protokoll)

| Check | Ergebnis |
| --- | --- |
| Build mit `-Wall -Wextra`, Apple Clang, `STORAGE=mysql+sqlite` | 0 Fehler, 447 Warnungen. Davon: ignored-qualifiers 159, overloaded-virtual 97, parentheses 75, deprecated 47, return-type 22, sign-compare 20 |
| Build-Stage des Dockerfiles (GCC 13.3, Ubuntu 24.04), Default-Flags | 117 Warnungen, davon 22× `-Wreturn-type` |
| `objdump` auf das GCC-Binary | `Commands::saveServer`: 0× `ret`. `Mailbox/Teleport/TrashHolder::__removeThing` liegen auf der Adresse von `_GLOBAL__sub_I_g_beds` |
| `objdump` auf das Apple-Clang-Binary | `Commands::saveServer` endet mit `mov w0,#0; ret`; harmlos |
| MariaDB 11.8.9 + `database.sql` | `sql_mode` strict. `SHOW TABLES` ohne `blessings`. INSERT aus `createPlayer` → ERROR 1364. `DELETE FROM blessings` → ERROR 1146 |
| `db.s3db` gegen `database.sqlite.sql` | Schema identisch. Seed: 2 Accounts, 3 Charaktere, darunter ein GM (access 5) mit Standardpasswort |
| glibc-Bedarf des 24.04-Binarys | `GLIBC_2.38` |
| `upchar` im Original (`0db70b7`) | Bytes intakt; 31 Paare, Abweichung bei `0xFF` |
| Lua-Tarball | 5.1.0 |
| Dateiverweise in `data/` | 966 XML-Verweise, 5 `dofile`-Pfade, Welt-Dateien aus `config.lua` und die in der Karte genannten Dateien: alle case-genau vorhanden |
| Seed im XML-Backend | `GM Xidaozu.xml` access 5; `222222.xml` mit demselben Passwort wie der Demo-Account |

Aufgeräumt: Container `evo-review-mariadb`, Image `evo-review-build` und alle Build- und Testdateien im
Scratchpad sind gelöscht. Das gezogene Image `mariadb:11` liegt weiter im Docker-Cache.

---

## 7. Prüfliste für Phase 2 (Abgleich mit den .md-Dateien)

Diese Aussagen stehen in Code-Kommentaren und Commit-Messages. Laut Code-Prüfung sind sie falsch, übertrieben oder
unvollständig. Beim Lesen der `.md`-Dateien gezielt gegenprüfen:

- [ ] „unchanged-in-behaviour“ / „1:1“: stimmt nicht überall, siehe `upchar` und `hexdump` (M1).
- [ ] `upchar`, „archived source corrupted to U+FFFD“: **falsch**.
- [ ] „builds clean on clang + GCC“: GCC meldet ohne Zusatzflags 117 Warnungen, 22× return-type, eine davon führt
      zum Absturz.
- [ ] „Verified: docker compose up end-to-end“: geprüft wurde nur der Start; Save und Charaktererstellung scheitern.
- [ ] „SQLite path working“ (`c954f15`): Das Schema ja, das Escaping nein.
- [ ] Branch `bugfixes` „crash-hardened, recommended“: Ihm fehlen die Fixes aus `master`.
- [ ] „no mechanic change“ (`5b2d774`): Die Depot-Änderung ist eine Mechanik-Änderung.
- [ ] „ASan-clean startup“: deckt nur den Start ab, nicht den Spielbetrieb.
- [ ] „GCC / Clang / MSVC“: MSVC wird nicht mehr gebaut.
- [ ] „SQLite::SQLite3 present on every CMake“: In CMake 4.x ist das deprecated.
- [ ] „self-contained download & run“: Das Linux-Paket braucht glibc ≥ 2.38.
- [x] „no call site depends on iteration order“: **stimmt** (geprüft).

## 8. Nicht geprüft / offen

- Den Windows-Build habe ich nicht lokal gebaut, nur die CI-Konfiguration gelesen.
- Es gab keinen Test mit einem echten 7.92-Client (Login, Spielbetrieb).
- Das XML-Backend habe ich zur Laufzeit nicht getestet.
- Die Remote-Control habe ich nicht gegen einen laufenden Server getestet.
- Ob die gebündelten macOS-Binaries korrekt signiert sind, ist offen.
- Den Inhalt von `data/` (Skripte, Monster, NPCs) habe ich nur auf Pfade geprüft.
- K1 ist inzwischen per Unit-Test gegen die DB-Schicht bestätigt (Datenverlust statt Injection, siehe Korrektur in K1).

---

## 9. Phase 2: Abgleich mit den .md-Dateien

Gelesen: `README.md`, `MODERNIZATION.md`, `DEPLOYMENT.md`, `CRASHFIXES.md` (Branch `bugfixes`) sowie die
Memory-Notizen der früheren Sitzung.

**Was sich bestätigt hat:**

- **Die zwei Varianten sind gewollt.** `master` ist die „clean 1:1“-Variante; bekannte Engine-Crashes bleiben dort
  bewusst erhalten. `bugfixes` ist die gehärtete Variante und der Standard in den Eggs. Das Konzept bleibt bestehen.
- **„no call site depends on iteration order“** (`MODERNIZATION.md` §3.1) stimmt.

**Was sich als falsch oder unvollständig herausgestellt hat:**

- **`upchar` (`MODERNIZATION.md` §4.1 und §5):** „corrupted to U+FFFD … ÿ → Ÿ“ ist falsch. Die Datei nennt selbst
  die Bedingung für eine Prüfung („If an authentic, uncorrupted … `tools.cpp` is available, the table can be diffed
  against it“). Diese Bedingung ist erfüllt: Der Init-Commit `0db70b7` *ist* diese Datei. Die Original-Tabelle ist
  exakt „E0..FF minus 0x20, ohne F7“, also ÿ → ß (0xDF).
- **Lua-Version:**
  - Die Entscheidungsnotiz der früheren Sitzung legt **Lua 5.1.5** fest. `MODERNIZATION.md` sagt nur „PUC Lua 5.1“.
    Eingebaut ist **5.1.0**.
  - Das Original wurde gegen Lua 5.1 gelinkt (`Makefile.win`: `others/lua-5.1/lib`). Im Repo liegt dazu
    `source/libraries/lua-5.1.2-1dre.DevPak`. Die `lua.dll` im Repo-Root ist ein 5.0.2-Überbleibsel.
  - 5.1.0 ist damit **älter** als das Original.
- **Compiler-Aussage:** „GCC 13+, Clang 16+, MSVC 2022 … without changing runtime behaviour“ (`MODERNIZATION.md`,
  `README.md`) hält nicht. MSVC wird nicht gebaut, und der GCC-Build hat UB, die zum Absturz führt (K3).
- **`DBResult::clear()`** (`MODERNIZATION.md` §4.5): Der `delete`-Fix stimmt, das Leck wurde übersehen (M2).
- **Accounts (`DEPLOYMENT.md`):** Genannt wird nur der Demo-Account 111111/tibia. Dass der GM-Account 222222 dasselbe
  Passwort hat, steht dort nicht (K2).
- **`GIT_REF` (`DEPLOYMENT.md`):** „`bugfixes` (crash-hardened, default)“ – genau diesem Branch fehlt der
  SQLite-Fix (H1).
- **`CRASHFIXES.md`:** „does not change game mechanics“ stimmt bei #7 (Depot) nicht. Die Ehrlichkeitsnotiz zu
  #8/#9 („nicht live bestätigt“) ist dagegen korrekt.
- **Memory-Notiz „Verified … server starts with zero SQL errors“:** stimmt nur für den Start. Speicherpfad und
  Blob-Pfad wurden nicht geprüft.

**Neuer Befund beim Planen der Fixes (gehört zu K1):** Im SQLite-Pfad ignoriert
`escapeString(const char*, size)` die Länge (`std::string(s)`). Binärdaten werden dadurch am ersten Null-Byte
abgeschnitten, und gelesen wird per `strlen`. Haus-Item-Attribute (`tileitems.attributes`) werden beim Speichern
also beschädigt. [Code]

## 10. Umsetzung (Branch `fix/review-findings`)

Basis ist `master`, ein Commit pro Befund. Der Branch ist **weder gepusht noch gemergt**.

| Befund | Commit | Umsetzung | Wie geprüft |
| --- | --- | --- | --- |
| K3 | `528f604` | Returns an den 14 Stellen, `-Werror=return-type` | GCC 16 lokal, GCC 11 und 13 in Docker: 0 Warnungen; `saveServer` endet mit `ret` |
| K1 (+M2, M5) | `149c558` | `prepare`/`step`, echte Längen, `''`-Quoting, `X'..'`, Escaping zur Laufzeit je Backend | Unit-Test besteht hier und scheitert auf `master`; Rune mit `0x27`/`0x00` in den Attributen übersteht zwei Server-Zyklen byte-genau |
| K4 | `84fcee5` | `blessings`, Spalten-Defaults, `sql_mode` per `MYSQL_INIT_COMMAND` | MariaDB 11.8: neues Schema im Strict-Mode ok, altes Schema mit dem Session-Modus ok; Docker-Stack speichert |
| K2 | `4cc618d` | GM-Account bleibt (**dein Wunsch**); Warnung beim Start | SQLite, XML, MySQL: Warnung bei Standardpasswort, keine nach Änderung |
| H2 | `2301771` | SIGTERM/SIGINT → Save → Exit 0 | Smoke-Test SQLite/XML (macOS, Ubuntu 22.04); `docker compose stop` in 1 s, Exit 0 |
| K5 (neu) | `ad22e81` | XML-Deadlock beim Speichern unter Linux/macOS | vom Smoke-Test gefunden, danach grün |
| M1 | `d0a1e38` | Original-`upchar`-Tabelle, Hexdump `0xB7` | alle 256 Bytes gegen `0db70b7` |
| M9 | `915e864` | Lua 5.1.5 | SHA256 gegen lua.org; Startup-Log identisch zu 5.1.0 |
| M8 | `6d8c0f4` | CMake-Aufräumen | 0 CMake-Warnungen in allen vier Varianten |
| H3, M6 | `99b372d` | Reinstall-Schutz, zweistufiges Escaping | Kopierlogik isoliert; Escaping mit BSD- und GNU-`sed` plus Lua 5.1 |
| H4, M7 | `4890723` | CI: `ubuntu:22.04`-Container, Smoke-Tests, brew-Prüfung | CI-Schritte lokal in `ubuntu:22.04` nachgestellt; Binary braucht GLIBC_2.34 |
| M10 | `9974f3e` | Remote-Control: eigenes Makro, Latin-1 | Build gegen wxWidgets 3.3 (macOS); GUI nicht gestartet |
| Niedrig | `97e35ef` | `intptr_t`-Cast, String + Enum | mit `-Wall` keine dieser Warnungen mehr |

**Abweichungen vom ursprünglichen Plan:**

- **K2:** Der GM-Seed bleibt wie gewünscht; statt ihn zu entfernen, warnt der Server beim Start.
- **`logintries` bleibt 0.** Mit `logintimeout = 0` sperrt die Logik nie. Außerdem zählt die Engine auch
  erfolgreiche Logins, und jeder Login besteht aus zwei Verbindungen. Mit `logintries = 3` würde schon ein Relog
  innerhalb einer Minute sperren. Wie man es gezielt einschaltet, steht in `DEPLOYMENT.md`.
- **H4:** `debian:11` als Build-Basis scheiterte (LTS-Ende, Security-Repo liefert 404). Deshalb `ubuntu:22.04`.

**Neu gefunden während der Umsetzung:**

- K5 (XML-Deadlock)
- die Korrektur von K1 (Datenverlust statt Injection)
- zwei verwaiste `tileitems`-Zeilen im Seed: deren `tileid` fehlt in `tilelist`, beim ersten Save fallen sie weg
  (harmlos)

**Sync nach `bugfixes` (erledigt am 2026-10-11):**

- `master` wurde in `bugfixes` gemergt. Konflikte: Bei Remote-Control und `MODERNIZATION.md` gilt die
  `master`-Fassung. In `release.yml` bleibt die Versionierung von `bugfixes` und gilt jetzt auch für den
  Linux-Job.
- Auf `bugfixes` liegen zusätzlich M3 (`listOnline` unter `gameLock`) und M4 (eigene Socket-Kopie pro Thread).
- **M4 gemessen:** 300 parallele Status-Abfragen werden mit dem Fix 300/300 beantwortet, auf `master` nur
  215–219/300. Das betrifft jede fünfte Verbindung eines Schwalls, etwa Logins nach einem Neustart. Deshalb
  spricht viel dafür, M4 auch nach `master` zu übernehmen. **Offen, deine Entscheidung.**
- `CRASHFIXES.md` auf `bugfixes`: #10 und #11 ergänzt, #7 als Mechanik-Änderung benannt.

**Weiter offen:**

- Test-Client für das 7.92-Protokoll (bewusst nicht umgesetzt).
- Windows ist nur über die CI gebaut; der Ctrl+C-Handler ist dort ungetestet. Offen ist auch die Signatur der
  gebündelten macOS-Binaries.
