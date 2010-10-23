#!/bin/sh

mypath=`readlink -f "$0"`
mydir=`dirname "${mypath}"`
basedir=`dirname "${mydir}"`
dir="${basedir}/local-tournament-1"
arbiter="${basedir}/arbiter/arbiter"

if [ ! -d "$dir" ]; then
	echo "Missing target directory $dir!"
	exit 1
fi

if [ ! -x "$arbiter" ]; then
	echo "Missing arbiter executable $arbiter!\n"
	exit 1
fi

$arbiter \
	--rounds=12 \
	--log="$dir"/log \
	--msg="$dir"/msg \
	./player \
	players/dvonner \
	players/dDvonn \
	players/holtz \
	| tee "$dir"/results.txt
