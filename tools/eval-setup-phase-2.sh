#!/bin/sh -e

# Quick hack script based on similar principes as eval-setup-phase.sh: tries to
# evaluate the quality of a start-up position from a bunch of logfiles (passed
# as arguments).

dir=`dirname "$0"`
tmpfile=`mktemp`
for transcript in "$@"
do
	# Once with Dvonner:
	"$dir"/dvonn-tool.py --transcript="$transcript" --output=logfile >"$tmpfile"
	winner=`"$dir"/eval-setup-phase.sh "$tmpfile"`
	printf "%s" "$winner"

	# Four times with my player:
	for repeat in `seq 4`; do
		state=`"$dir"/dvonn-tool.py --transcript="$transcript" --truncate=49 --output=state`
		moves=`./player --state="$state" --color=3 --eval=$((6000 + 1000*$repeat)) 2>/dev/null`
		winner=`"$dir"/dvonn-tool.py --transcript="$transcript" --truncate=49 $moves --output=winner`
		printf '\t%s' "$winner"
	done
	printf "\n"
done
rm -f "$tmpfile"
