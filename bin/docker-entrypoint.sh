#!/usr/bin/env bash
# *****************************************************************************
# This file is part of dirtsand.
#
# dirtsand is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# dirtsand is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.
# *****************************************************************************

DS_CONFIG_FILE=/opt/dirtsand/etc/dirtsand.ini
CLIENT_CONFIG_FILE=/opt/dirtsand/etc/server.ini

# Begin code copypasta'd from Deledrius
# Check for existing configuration files and read existing keys if available, otherwise generate a new set
echo -e "\e[36mPreparing game server...\e[0m"
if [[ ! -f "$DS_CONFIG_FILE" || ! -f "$CLIENT_CONFIG_FILE" ]]; then
    echo -e "\e[33mExisting configuration files not found or are incomplete.  Generating new configuration...\e[0m"
    keyset=`/opt/dirtsand/bin/dirtsand --generate-keys`
else
    echo -e "\e[32mUsing existing configuration as a template.\e[0m"
    keyset=`cat "$DS_CONFIG_FILE" "$CLIENT_CONFIG_FILE"`
fi

# Parse the keys we've either harvested or generated
regex="([a-zA-Z]+\.[a-zA-Z]+\.[a-zA-Z])[ =\"]+([a-zA-Z0-9+=\/]+)\"*"
while [[ $keyset =~ $regex ]]; do
    keyname=${BASH_REMATCH[1]}
    keyvalue=${BASH_REMATCH[2]}
    case "$keyname" in
        "Key.Auth.N")
            kAuthN="$keyvalue"
            ;;

        "Key.Auth.K")
            kAuthK="$keyvalue"
            ;;

        "Server.Auth.X")
            kAuthX="$keyvalue"
            ;;

        "Key.Game.N")
            kGameN="$keyvalue"
            ;;

        "Key.Game.K")
            kGameK="$keyvalue"
            ;;

        "Server.Game.X")
            kGameX="$keyvalue"
            ;;

        "Key.Gate.N")
            kGateN="$keyvalue"
            ;;

        "Key.Gate.K")
            kGateK="$keyvalue"
            ;;

        "Server.Gate.X")
            kGateX="$keyvalue"
            ;;
    esac
    i=${#BASH_REMATCH}
    keyset=${keyset:i}
done
# End DeledriusLand

echo -e "\e[36mWriting configuration...\e[0m"

# Write new dirtsand.ini
cat << EOF > "${DS_CONFIG_FILE}"
# NOTE: This file was automatically generated by docker-entrypoint.sh when you ran dockersand start
# If you need to change anything, modify your compose file and restart the container.

# Server encryption keys
Key.Auth.N = ${kAuthN}
Key.Auth.K = ${kAuthK}
Key.Gate.N = ${kGateN}
Key.Gate.K = ${kGateK}
Key.Game.N = ${kGameN}
Key.Game.K = ${kGameK}

# For Python/SDL
Key.Droid = ${DS_DROIDKEY:-31415926535897932384626433832795}

# EXTERNAL server addresses
File.Host = ${DS_HOST:-127.0.0.1}
Auth.Host = ${DS_HOST:-127.0.0.1}
Game.Host = ${DS_HOST:-127.0.0.1}

# Server address/port to bind to...  Leave commented to bind to any
# address on the default MOULa port (14617)
Lobby.Addr = 0.0.0.0
Lobby.Port = 14617

# JSON Server Status over HTTP.  Note:  Disabling the status server will
# also disable the welcome message below.
Status.Enabled = true
Status.Addr = 0.0.0.0
Status.Port = 8080

# Paths to server data
File.Root = /opt/dirtsand/lib/filesrv
Auth.Root = /opt/dirtsand/lib/authsrv
Sdl.Path = /opt/dirtsand/lib/SDL
Age.Path = /opt/dirtsand/lib/dat

# Postgres options
Db.Host = ${POSTGRES_HOST:-db}
Db.Port = ${POSTGRES_PORT:-5432}
Db.Username = ${POSTGRES_USER}
Db.Password = ${POSTGRES_PASSWORD}
Db.Database = ${POSTGRES_DB}

# The default Welcome message -- This can be changed while the server
# is running with the welcome command
Welcome.Msg = ${SHARD_WELCOME_MSG:-"It's ALIVE!"}
EOF

# The static_ages.ini file needs to be in the same directory as dirtsand.ini
if [[ ! -f "/opt/dirtsand/etc/static_ages.ini" ]]; then
    cp "/opt/dirtsand/static_ages.ini" "/opt/dirtsand/etc/static_ages.ini"
fi

# Write client ini
cat << EOF > "${CLIENT_CONFIG_FILE}"
# This is the server.ini file for plClient. It was automatically generated when you started the container.
# To change the values, set the appropriate environment variables in the compose file, then
# restart dockersand.

# Server status page shown in the launcher window.
Server.Status "${SHARD_STATUS_URL:-http://${DS_HOST:-127.0.0.1}:8080/welcome}"

# Shard front-end server address.
Server.Gate.Host "${DS_HOST:-127.0.0.1}"
Server.Auth.Host "${DS_HOST:-127.0.0.1}"
Server.Port ${SHARD_PORT:-14617}

# Shard name - NOTE: this is currently not visible anywhere.
Server.DispName "${SHARD_NAME:-My DIRTSAND Shard}"

# URL to a server signup page. DIRTSAND does not provide this by default.
Server.Signup "${SHARD_SIGNUP_URL:-http://example.com/create_account}"

# Crypto keys (do not modify)
Server.Auth.N "${kAuthN}"
Server.Auth.X "${kAuthX}"
Server.Game.N "${kGameN}"
Server.Game.X "${kGameX}"
Server.Gate.N "${kGateN}"
Server.Gate.X "${kGateX}"
EOF

echo -e "\e[32m... Success!\e[0m"

# Ensure the age and SDL files are present.
# due to mount-point hiding and to ensure a sane environment in case the user broke something...
echo -e "\e[36mChecking for age and SDL files...\e[0m"
if [[ ! "$(ls -A /opt/dirtsand/lib/dat)" ]]; then
    echo -e "\e[33mAge files missing. Copying from base image...\e[0m"
    cp -R "/opt/dirtsand/lib/moul-scripts/dat/." "/opt/dirtsand/lib/dat"
fi
if [[ ! "$(ls -A /opt/dirtsand/lib/SDL)" ]]; then
    echo -e "\e[33mSDL files missing. Copying from base image...\e[0m"
    cp -R "/opt/dirtsand/lib/moul-scripts/SDL/." "/opt/dirtsand/lib/SDL"
fi
echo -e "\e[32m... Success!\e[0m"

# If the PostgreSQL database has to init, we generally finish this script before psql is up.
# Therefore, we wait until Postgres is accepting connections.
echo -e "\e[36mWaiting for database availability...\e[0m"
until pg_isready -q -h db -d "$POSTGRES_DB" -U "$POSTGRES_USER"
do
    sleep 0.1
done

echo -e "\e[36m(Re-)Initializing database..\e[0m"
psql -d "host=${POSTGRES_HOST:-db} port=${POSTGRES_PORT:-5432} dbname=${POSTGRES_DB} user=${POSTGRES_USER} password=${POSTGRES_PASSWORD}" -c "CREATE EXTENSION IF NOT EXISTS \"uuid-ossp\";"
psql -d "host=${POSTGRES_HOST:-db} port=${POSTGRES_PORT:-5432} dbname=${POSTGRES_DB} user=${POSTGRES_USER} password=${POSTGRES_PASSWORD}" -f /opt/dirtsand/db/dbinit.sql
psql -d "host=${POSTGRES_HOST:-db} port=${POSTGRES_PORT:-5432} dbname=${POSTGRES_DB} user=${POSTGRES_USER} password=${POSTGRES_PASSWORD}" -f /opt/dirtsand/db/functions.sql

echo -e "\e[36mStarting DIRTSAND...\e[0m"
exec "/opt/dirtsand/bin/dirtsand" "$DS_CONFIG_FILE"
