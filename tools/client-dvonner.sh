#!/bin/sh -e

path=`readlink -f "$0"`
cd "`dirname "$path"`"
./client.bash './play-dvonner.tcl ../dvonner/dvonner' $@
