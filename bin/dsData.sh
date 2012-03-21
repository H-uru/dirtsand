#!/bin/bash
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
# MOUL DataServer Manifest File line creator
# By Michael Hansen
#
# Version 1.0

E_NOARGS=65
E_FILENOTEXIST=1

NUMFILES=0
QUIETMODE=false

noArgs() {
    if ! $QUIETMODE ; then
        echo "Usage: `basename $0` [options] filename"
        echo
        echo "Options:"
        echo "    -f --flag Flag  Set the Flags to Flag (instead of the default)"
        echo "    -q --quiet      Quiet mode (don't print errors)"
        echo "    -? --help       You're lookin' at it..."
        echo
    fi
}

if [ -z "$1" ] ; then
    noArgs
    exit $E_NOARGS
fi

while [[ -n $1 ]] ; do
    case "$1" in
        "--help" | "-?")
            noArgs
            exit 0
            ;;
        "--flag" | "-f")
            FILEFLAG="$2"
            shift
            ;;
        "--quiet" | "-q")
            QUIETMODE=true
            ;;
        *)
            FILENAMES[$NUMFILES]="$1"
            NUMFILES=$(($NUMFILES+1))
            ;;
    esac
    shift
done

if [ "$NUMFILES" == 0 ] ; then
    noArgs
    exit $E_NOARGS
fi


idx=0
while [ $idx -lt $NUMFILES ] ; do

FILENAME="${FILENAMES[idx]}"

if [ ! -e "$FILENAME" ] ; then
    if ! $QUIETMODE ; then
        echo "`basename $0`: File $FILENAME does not exist!"
    fi
    idx=$(($idx + 1))
    continue
fi

if [ ! -f "$FILENAME" ] ; then
    if ! $QUIETMODE ; then
        echo "`basename $0`: Skipping $FILENAME since it is not a regular file"
    fi
    idx=$(($idx + 1))
    continue
fi

FILESIZE=`du -b "$FILENAME" | cut -f 1`
FILESUM=`md5sum "$FILENAME" | cut -f 1 --delimiter=" "`

if [ -z "$FILEFLAG" ] ; then
    FILEFLAG="0"
fi

gzip -9 "$FILENAME"
FILESIZEGZ=`du -b "$FILENAME.gz" | cut -f 1`
FILESUMGZ=`md5sum "$FILENAME.gz" | cut -f 1 --delimiter=" "`

echo "$FILENAME,$FILENAME.gz,$FILESUM,$FILESUMGZ,$FILESIZE,$FILESIZEGZ,$FILEFLAG"

idx=$(($idx + 1))
done
