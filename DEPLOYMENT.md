# Deployment

Three ways to run the modernized Evolution 0.7.8 server.

## 1. Docker Compose (MySQL, one command)

Brings up the server **and** a MariaDB database, with the schema loaded automatically:

```bash
docker compose up --build
```

- First start creates the database from `database.sql` (demo account **111111 / tibia**).
- Connect a Tibia 7.92 client to `127.0.0.1:7171` (login and game share one port).
- For remote players, set `SERVER_IP` in `docker-compose.yml` to your public/host IP.
- DB credentials and connection are set via the `server` service environment
  (`SQL_HOST/SQL_USER/SQL_PASS/SQL_DB`, optional `SQL_PORT`). The image (`Dockerfile`)
  builds the MySQL backend from source; override with `--build-arg STORAGE=sqlite`.

The server runs as a non-root user (the engine refuses to run as root).
`docker compose stop` (SIGTERM) makes the server save all players and the world
before it exits; `stop_grace_period: 60s` gives it the time to do so.

## 2. Pterodactyl eggs

Two eggs under `pterodactyl/`:

- **`egg-evolution-sqlite.json`** — self-contained, no external database.
- **`egg-evolution-mysql.json`** — uses a customer database (create one in the panel,
  fill in the `SQL_*` variables; the schema is imported into it on install).

Both **build from source on install** into the server's file area, so **`config.lua`
and the entire `data/` tree (scripts, map, monsters, NPCs, …) are editable** via the
panel file manager and SFTP — customize your server freely.

Common settings are exposed as **panel variables** (public IP, world name, world type,
max players, exp/skill/loot/magic/spawn rates, and for MySQL the DB connection). They
are applied to `config.lua` on each start via `start.sh` — **only when set**, so any
value you leave blank keeps whatever you edited in `config.lua` directly. The listen
port follows the Pterodactyl allocation.

`GIT_REF` selects what to build: `bugfixes` (crash-hardened, default), `master`
(clean 1:1), or a tag. Image: `ghcr.io/pterodactyl/yolks:debian`.

**Reinstall** updates the binary, its libraries and `start.sh`, but keeps your
`config.lua`, `data/` and `db.s3db`: those are only added when they are missing.
To pick up changed scripts from a new version, delete the files in question first.
For the MySQL egg, a reinstall also creates the `blessings` table if your database
predates it. **Stop** (Ctrl+C / SIGINT) saves all players and the world before exit.

## 3. Prebuilt release packages

The GitHub Releases (`v0.7.8`, `v0.7.9-bugfix`) ship self-contained per-OS `.zip`
packages — download, unzip, run `./evolutions` (or `run.bat` / `run.sh`). See the
package's `README-PACKAGE.txt`. The Linux build needs glibc 2.35 or newer
(Ubuntu 22.04, Debian 12, RHEL 9 and later). When you update by unpacking a newer
package, do not overwrite your `db.s3db` (SQLite) or `data/` (XML) — they hold your
players.

## Accounts and passwords

- **111111 / tibia** — the Account Manager. Public on purpose: players log in with
  it to create their own account and characters.
- **222222 / tibia** — the GM account (`GM Xidaozu`, access 5) shipped with the
  original data. As long as it still has the default password, the server prints
  a warning on every start. **Change it before your server is reachable from the
  internet**, e.g. `UPDATE accounts SET password = 'new-password' WHERE accno = 222222;`
  (MySQL, or SQLite via `sqlite3 db.s3db`), or the `pass` attribute in
  `data/accounts/222222.xml` (XML backend). Passwords are stored in plain text.
- **Making a character a GM:** create it through the Account Manager, then set its
  access level, e.g. `UPDATE players SET access = 5 WHERE name = 'Name';`
  (XML backend: the `access` attribute in `data/players/<Name>.xml`).

## Upgrading an existing MySQL database

Databases created from an older `database.sql` lack the `blessings` table, so every
player save fails. Add it once:

```sql
CREATE TABLE IF NOT EXISTS `blessings` (
  `player` int(11) NOT NULL default '0',
  `id` int(11) NOT NULL default '0',
  KEY `player` (`player`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
```

The server sets its session to the non-strict mode of MySQL 5.0 itself, so no
`sql_mode` change on the database server is needed.

---

### Notes

- This engine serves login and game on a **single port** (default 7171).
- World file names in `config.lua` use exact case (`Evolutions.otbm`), required on
  case-sensitive Linux filesystems.
- `sql_port` (default 3306) is supported for MySQL/MariaDB on a non-standard port.
- Login throttling is off in the shipped `config.lua` (`logintries = 0`). To enable
  it, set **both** `logintries` and `logintimeout` (in ms; with `logintimeout = 0`
  nothing is ever blocked). Note that the engine also counts *successful* logins
  within `retrytimeout`, and every login is two connections (character list, then
  game): with `logintries = 3` a player who relogs once within a minute is blocked.
