#!/bin/sh
# Entrypoint for the Dockerized Evolution server.
# Patches the DB connection (and optionally the advertised IP / world name) in
# config.lua from environment variables, then launches the server. This lets the
# same image talk to the compose `db` service or an external/customer MySQL.
set -e
cd /opt/evolution

# --- helpers: replace `key = ...` in config.lua --------------------------------
# A string value is escaped twice: for the Lua "..." literal (\ and "), then for
# the sed replacement (\ & |). Line breaks are dropped.
set_str() {  # set_str <lua_key> <value>
    esc=$(printf '%s' "$2" | tr -d '\r\n' | sed -e 's/[\\"]/\\&/g' -e 's/[&|\\]/\\&/g')
    sed -i "s|^[[:space:]]*$1[[:space:]]*=.*|$1 = \"${esc}\"|" config.lua
}
set_num() {  # set_num <lua_key> <digits>
    case "$2" in
        ''|*[!0-9]*) echo ":: ignoring non-numeric $1='$2'" ;;
        *) sed -i "s|^[[:space:]]*$1[[:space:]]*=.*|$1 = $2|" config.lua ;;
    esac
}

# --- database connection (only the MySQL build uses these) --------------------
set_str sql_host "${SQL_HOST:-db}"
set_str sql_user "${SQL_USER:-evolution}"
set_str sql_pass "${SQL_PASS:-evolution}"
set_str sql_db   "${SQL_DB:-evolution}"
set_num sql_port "${SQL_PORT:-3306}"

# --- optional: advertised IP for clients, world name --------------------------
# The login server tells the client which IP to use for the game connection;
# set SERVER_IP to your public/host IP for remote players (default keeps config).
[ -n "${SERVER_IP}" ]  && set_str ip "${SERVER_IP}"
[ -n "${WORLD_NAME}" ] && set_str worldname "${WORLD_NAME}"

echo ":: starting Evolution (sql_host=${SQL_HOST:-db}, sql_db=${SQL_DB:-evolution})"
exec ./evolutions "$@"
