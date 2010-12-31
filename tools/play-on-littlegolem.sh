#!/bin/sh

path=`readlink -f "$0"`
dir=`dirname "$path"`

login_url=http://www.littlegolem.net/jsp/login/index.jsp
game_url=http://www.littlegolem.net/jsp/game/game.jsp
game_list_url=http://www.littlegolem.net/jsp/game/index.jsp
user=PrimaDvonna
pass=pihuwymo

dvonntool=$dir/dvonn-tool.py
player=$dir/../player
player_params="--eval=100000"

if [ ! -x "$dvonntool" ]; then echo "dvonn-tool not found!"; exit 1; fi
if [ ! -x "$player" ]; then echo "player not found!"; exit 1; fi

msg=`curl -L -s -c cookies.txt -F login="$user" -F password="$pass" "$login_url" | grep -o 'You have successfully logged in'`
if [ -z "$msg" ]; then echo "Login failed!"; exit 1; fi

gids=`curl -L -s -b cookies.txt "$game_list_url" | \
	sed -e "/Games where it is your opponent's turn/ q" | \
	grep -o 'game.jsp?gid=[0-9]*' | cut -d= -f2`

for gid in $gids
do
	state=`"$dvonntool" --littlegolem="$gid" | grep '^[ABCD]'`
	if [ -z "$state" ]; then
		echo "Couldn't grab state for game $gid\!"
		exit 1
	fi
	
	if [ -n "$state" ]; then
		move=`"$player" $player_params --analyze --state="$state" | tr 12345A-Z edcbaa-z`
		curl -L -s -b cookies.txt -F sendgame="$gid" -F sendmove="$move" "$game_url" >/dev/null
	fi
done
