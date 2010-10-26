#!/bin/sh

mypath=`readlink -f "$0"`
mydir=`dirname "$0"`
dvonntool=$mydir/dvonn-tool.py
dvonner=`readlink -f $mydir/../players/dvonner.bin`

dvonner() {
	$dvonner <<EOF || exit1
/stop
/load $1
/players 0
/start
/quit
EOF
}

logfile=$1
if [ ! -f "$logfile" ]; then
	echo "Missing logfile argument!"
	exit 1
fi

player1=`grep '^# Player 1: ' "$logfile" | cut -c 13-`
player2=`grep '^# Player 2: ' "$logfile" | cut -c 13-`
if [ -z "$player1" ] || [ -z "$player2" ]; then
	echo "Couldn't parse logfile!"
	exit 1
fi

moves=`grep -v '^#' "$logfile" | head -2`
if [ "`echo $moves | wc -w`" != 49 ]; then
	echo "Unexpected number of moves in logfile!"
	exit 1
fi
save=`mktemp tmp-XXXX.dvg`
$dvonntool --output=dvonner $moves >$save || exit 1
winner=`dvonner | grep 'The game ended' | egrep -o 'BLACK|WHITE'`
rm -f "$save"
if [ "$winner" = WHITE ]; then echo $player1; fi
if [ "$winner" = BLACK ]; then echo $player2; fi
if [ -z "$winner" ]; then echo "No winner! (Tie?)"; fi
