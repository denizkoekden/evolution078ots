#!/bin/sh
# Entrypoint for the Dockerized Evolution server.
# Patches the DB connection (and optionally the advertised IP / world name) in
# config.lua from environment variables, then launches the server. This lets the
# same image talk to the compose `db` service or an external/customer MySQL.
set -e
cd /opt/evolution

# --- helper: replace `key = ...` (string value) in config.lua -----------------
set_str() {  # set_str <lua_key> <value>
    key="$1"; val="$2"
    # escape & and | for sed replacement
    esc=$(printf '%s' "$val" | sed -e 's/[&|\\]/\\&/g')
    sed -i "s|^[[:space:]]*${key}[[:space:]]*=.*|${key} = \"${esc}\"|" config.lua
}

# --- database connection (only the MySQL build uses these) --------------------
set_str sql_host "${SQL_HOST:-db}"
set_str sql_user "${SQL_USER:-evolution}"
set_str sql_pass "${SQL_PASS:-evolution}"
set_str sql_db   "${SQL_DB:-evolution}"
# sql_port is a number (no quotes)
sed -i "s|^[[:space:]]*sql_port[[:space:]]*=.*|sql_port = ${SQL_PORT:-3306}|" config.lua

# --- optional: advertised IP for clients, world name --------------------------
# The login server tells the client which IP to use for the game connection;
# set SERVER_IP to your public/host IP for remote players (default keeps config).
[ -n "${SERVER_IP}" ]  && set_str ip "${SERVER_IP}"
[ -n "${WORLD_NAME}" ] && set_str worldname "${WORLD_NAME}"

echo ":: starting Evolution (sql_host=${SQL_HOST:-db}, sql_db=${SQL_DB:-evolution})"
exec ./evolutions "$@"
