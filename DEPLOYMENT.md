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

## 3. Prebuilt release packages

The GitHub Releases (`v0.7.8`, `v0.7.9-bugfix`) ship self-contained per-OS `.zip`
packages — download, unzip, run `./evolutions` (or `run.bat` / `run.sh`). See the
package's `README-PACKAGE.txt`.

---

### Notes

- This engine serves login and game on a **single port** (default 7171).
- World file names in `config.lua` use exact case (`Evolutions.otbm`), required on
  case-sensitive Linux filesystems.
- `sql_port` (default 3306) is supported for MySQL/MariaDB on a non-standard port.
