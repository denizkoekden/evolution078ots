#!/bin/bash
# Applies the Pterodactyl panel variables to config.lua, then launches the server.
# Only NON-EMPTY variables are applied, so any setting you leave blank keeps whatever
# you edited directly in config.lua via the file manager / SFTP. All of config.lua and
# the whole data/ tree (scripts, map, monsters, npc, ...) live here and stay editable.
cd /home/container
C=config.lua

# String values are escaped for the Lua "..." literal (\ and "), then for sed (\ & |).
sstr() {   # string value
    [ -z "$2" ] && return
    local esc
    esc=$(printf '%s' "$2" | tr -d '\r\n' | sed -e 's/[\\"]/\\&/g' -e 's/[&|\\]/\\&/g')
    sed -i "s|^[[:space:]]*$1[[:space:]]*=.*|$1 = \"${esc}\"|" "$C"
}
snum() {   # numeric value
    [ -z "$2" ] && return
    case "$2" in
        *[!0-9]*) echo ":: ignoring non-numeric $1='$2'" ;;
        *) sed -i "s|^[[:space:]]*$1[[:space:]]*=.*|$1 = $2|" "$C" ;;
    esac
}

# Network: bind port comes from the Pterodactyl allocation; PUBLIC_IP is what the
# login server advertises to clients (set it to your node's public IP for remote play).
snum port       "${SERVER_PORT}"
sstr ip         "${PUBLIC_IP}"

# World
sstr worldname  "${WORLD_NAME}"
sstr worldtype  "${WORLD_TYPE}"
snum maxplayers "${MAX_PLAYERS}"

# Rates
snum expmul     "${RATE_EXP}"
snum skillmul   "${RATE_SKILL}"
snum lootmul    "${RATE_LOOT}"
snum manamul    "${RATE_MAGIC}"
snum spawnmul   "${RATE_SPAWN}"

# Database (MySQL build only; harmless no-ops on the SQLite build)
sstr sql_host   "${SQL_HOST}"
snum sql_port   "${SQL_PORT}"
sstr sql_user   "${SQL_USER}"
sstr sql_pass   "${SQL_PASS}"
sstr sql_db     "${SQL_DB}"

exec ./evolutions
