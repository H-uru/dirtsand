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

# --- CONFIGURABLE ---
COMPOSE_FILE="docker-compose.yml"

# The directory this script is in. If you customize this file outside of the repo, hardcode the
# path to the DIRTSAND sources.
DS_DIR=$(dirname $(readlink -f "$0"))

# The prefix for the containers. Defaults to the name of the DIRTSAND source directory ("dirtsand"),
# so the containers are usually: dirtsand_moul_1 and dirtsand_db_1. Change this to something else
# if you intend to run multiple shards on the same machine.
PROJECT_NAME=$(basename $DS_DIR)

do_help() {
    echo "Available commands:"
    echo "    attach - attach to the DIRTSAND console"
    echo "    build - rebuild the DIRTSAND containers from the source and compose files"
    echo "    restart - restart the DIRTSAND containers"
    echo "    start - start the DIRTSAND containers"
    echo "    status - check if the DIRTSAND containers are running"
    echo "    stop - stop the DIRTSAND containers"
}

check_docker() {
    which docker > /dev/null
    if [[ $? -ne 0 ]]; then
        echo -e "\e[31mERROR:\e[0m docker is not installed."
        exit 1
    fi
    which docker-compose > /dev/null
    if [[ $? -ne 0 ]]; then
        echo -e "\e[31mERROR:\e[0m docker-compose is not installed."
        exit 1
    fi
}

# docker-compose requires the CWD have the docker-compose.yml file.
pushd "$DS_DIR" > /dev/null
trap "popd > /dev/null" EXIT

if [[ ! -n $1 ]];then
    echo "dockersand - DIRTSAND for Docker"
    do_help
elif [[ $1 = "help" ]]; then
    echo "dockersand - DIRTSAND for Docker"
    do_help
elif [[ $1 = "attach" ]]; then
    check_docker
    docker-compose ps | grep ".*_moul_[0-9]*" > /dev/null
    if [[ $? -ne 0 ]]; then
        echo -e "\e[33mWARNING\e[0m: dockersand is not running! Starting..."
        docker-compose -p $PROJECT_NAME -f $COMPOSE_FILE up -d
    fi
    CONTAINER_NAME=$(docker-compose ps | grep -oh ".*_moul_[0-9]*")
    if [[ $? -ne 0 ]]; then
        echo -e "\e[31mERROR\e[0m: Unable to determine DIRTSAND container name. Sorry :("
        exit 1
    fi
    echo "Attaching to DIRTSAND... Press Ctrl+P Ctrl+Q to detatch!"
    docker attach $CONTAINER_NAME
elif [[ $1 = "build" ]]; then
    check_docker
    docker-compose ps | grep ".*_moul_[0-9]*" > /dev/null
    RUNNING=$?
    if [[ $RUNNING -eq 0 ]]; then
        echo -e "\e[36mStopping dockersand...\e[0m"
        docker-compose down
    fi
    docker-compose -p $PROJECT_NAME -f $COMPOSE_FILE build
    if [[ $? -ne 0 ]]; then
        echo -e "\e[31mFAILED\e[0m"
        exit 1
    fi
    if [[ $RUNNING -eq 0 ]]; then
        echo -e "\e[36mStarting dockersand...\e[0m"
        docker-compose -p $PROJECT_NAME -f $COMPOSE_FILE up -d
        if [[ $? -eq 0 ]]; then
            echo -e "\e[32mSUCCESS\e[0m: To manage DIRTSAND, use ./dockersand.sh attach"
        else
            echo -e "\e[31mFAILED\e[0m"
        fi
    fi
elif [[ $1 = "restart" ]]; then
    check_docker
    docker-compose ps | grep dirtsand > /dev/null
    if [[ $? -eq 0 ]]; then
        echo -e "\e[36mRestarting dockersand...\e[0m"
        docker-compose restart
    else
        echo -e "\e[33mWARNING\e[0m: dockersand is not running! Starting..."
        docker-compose -p $PROJECT_NAME -f $COMPOSE_FILE up -d
    fi
    if [[ $? -eq 0 ]]; then
        echo -e "\e[32mSUCCESS\e[0m: To manage DIRTSAND, use ./dockersand.sh attach"
    else
        echo -e "\e[31mFAILED\e[0m"
    fi
elif [[ $1 = "start" ]]; then
    check_docker
    docker-compose ps | grep ".*_moul_[0-9]*" > /dev/null
    if [[ $? -ne 0 ]]; then
        echo -e "\e[36mStarting dockersand...\e[0m"
        docker-compose -p $PROJECT_NAME -f $COMPOSE_FILE up -d
        if [[ $? -eq 0 ]]; then
            echo -e "\e[32mSUCCESS\e[0m: To manage DIRTSAND, use ./dockersand.sh attach"
        else
            echo -e "\e[31mFAILED\e[0m"
        fi
    else
        echo -e "\e[33mWARNING\e[0m: dockersand is already running!"
    fi
elif [[ $1 = "status" ]]; then
    check_docker
    docker-compose ps | grep ".*_moul_[0-9]*" > /dev/null
    if [[ $? -eq 0 ]]; then
        echo -e "dockersand is \e[32mUP\e[0m"
    else
        echo -e "dockersand is \e[31mDOWN\e[0m"
    fi
elif [[ $1 = "stop" ]]; then
    check_docker
    docker-compose ps | grep ".*_moul_[0-9]*" > /dev/null
    if [[ $? -eq 0 ]]; then
        echo -e "\e[36mStopping dockersand...\e[0m"
        docker-compose down
    else
        echo -e "\e[33mWARNING\e[0m: dockersand is not running!"
    fi
else
    echo -e "\e[31mERROR\e[0m: Unrecognized option $1"
    do_help
fi
