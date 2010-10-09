#!/bin/bash

DIR=../codecup

grep_result() {
	grep Result: "$TMP" | egrep -o '(BLACK|WHITE) (WIN|ILLEGAL|TIMEOUT|CRASH|EXIT)|DRAW'
}

grep_param() {
	sed 's/\\"/"/g' "$TMP" | grep -o "<param name=\"$1\" value=\"[^\"]*\">" | cut -d\" -f4
}

format_game() {
	echo "Event [Competition ${CO} Round ${CR}]"
	echo "Site [www.codecup.nl]"
	echo "White [$white]"
	echo -n "Black [$black]"
	
	local i=0
	local pl=0
	for move in $moves
	do
		if [ $i != 48 ]; then
			if [ $pl = 0 ]; then
				echo
				echo -n "`expr $i / 2 + 1`."
			fi
			echo -n " $move"
			pl=`expr 1 - $pl`
		fi
		i=`expr $i + 1`
	done
	echo ''
}

scrape_game() {
	GA=$1

	if ! curl -s -o "$TMP" "http://www.codecup.nl/showgame.php?ga=$GA"; then
		echo "Could not fetch page for game ${GA}!"
		return 1
	fi

	result=`grep_result`
	white=`grep_param white`
	black=`grep_param black`
	moves=`grep_param moves | tr ',A-Z' ' a-z' `

	if [ -z "$result" ] || [ -z "$white" ] || [ -z "$black" ] || [ -z "$moves" ]; then
		echo "Could not parse page for game ${GA}!"
		return 1
	fi

	format_game >"${DIR}/${CO}-${CR}-${GA} ${white} vs ${black}: $result.txt"
}

scrape_round() {
	CR=$1

	if ! curl -s -o "$TMP" "http://www.codecup.nl/competitionround?cr=${CR}"; then
		echo "Could not fetch page for round ${CR}!"
		return 1
	fi

	games=`grep -o 'showgame.php?ga=[0-9]*' "$TMP" | cut -d= -f2`
	if [ -z "$games" ]; then
		echo "Could not parse page for round ${CR}!" 
		return 1
	fi

	for game in $games; do
		if ! scrape_game $game; then
			echo "Could not scrape game ${game}"
			return 1
		fi
	done
}

scrape_competition() {
	CO=$1

	if ! curl -s -o "$TMP" "http://www.codecup.nl/competition.php?comp=${CO}"; then
		echo "Could not fetch page for competition ${CO}!"
		return 1
	fi

	rounds=`grep -o 'competitionround.php?cr=[0-9]*' "$TMP" | cut -d= -f2`
	if [ -z "$rounds" ]; then
		echo "Could not parse page for competition ${CO}!" 
		return 1
	fi

	for round in $rounds; do
		if ! scrape_round $round; then
			echo "Could not scrape round ${round}!"
			return 1
		fi
	done

	return 0
}

if ! test -d "$DIR"; then
	echo "${DIR} is not a directory!"
	exit 1
fi

if test -z "$1"; then
	echo "Missing argument: competition id"
	exit 1
fi

TMP=`mktemp`
scrape_competition "$1"
rm -f "$TMP"
