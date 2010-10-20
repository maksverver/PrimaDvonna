#!/usr/bin/env expect

# Wraps dvonner (a Dvonn AI written by Matthias Bodenstein, see:
# http://matztias.de/spiele/dvonner/dvonner-e.htm) with expect to make it
# conform to the CodeCup player interface.

# Optional arguments:
#  1: path to dvonner (default: dvonner)
#  2: difficulty level (default: 5000)

proc msg {txt} {send_error "$txt\n"}
proc quit {} {msg "Quit received."; exit 0}
proc die {} {msg "Expect failed!"; exit 1}

# Parse arguments:
set path [lindex $argv 0]
set level [lindex $argv 1]

if {$path eq ""} {set path dvonner}
if {$level eq ""} {set level 5000}

# Set expect paramters. Timeout should be long (to allow for thinking).
# log_user must normally be 0, but can be set to 1 for manual testing.
set timeout 600
log_user 0

# Run a small test to see if Dvonner works as expected:
msg "Testing Dvonner..."
spawn $path -config=/dev/null -help
expect {Dvonner V1.* by Matthias Bodenstein} {close} default {die}

# Read first line:
msg "Waiting for user..."
expect_user {
	Quit {quit}
	Start {
		# AI must start, so player is black
		set color black
		msg "User declined to start."
	}
	-re {([A-K][1-5])} {
		# Player has started, so he plays white
		set color white
		set last_move $expect_out(1,string)
	}
	default {exit 1}
}

# Start Dvonner:
spawn $path -config=/dev/null -color=$color -setup=n -players=1 -level=$level

if {$color eq "white"} {
	msg "Sending user's move ($last_move) to Dvonner."
	expect {your move:} {send "$last_move\n"} {default} {exit 1}
	set turn 1
	set player black
} else {
	set turn 0
	set player white
}

# Main loop
while true {
	if {$player eq $color} {
		# Read a move from the user:
		msg "Waiting for user..."
		if {$turn < 49} {
			expect_user {
				-re {([A-K][1-5])} {
					set last_move $expect_out(1,string) }
				Quit {quit}
				default {die}
			}
		} else {
			expect_user {
				-re {([A-K][1-5][A-K][1-5]|PASS)} {
					set last_move $expect_out(1,string) }
				Quit {quit}
				default {die}
			}
		}
		# Send move to Dvonner:
		msg "Sending user's move ($last_move) to Dvonner."
		expect {
			{your move:} {send "$last_move\n"}
			default {die}
		}
	} else {
		# Read a move from the AI:
		msg "Waiting for Dvonner..."
		if {$turn < 49} {
			expect {
				-re {Dvonner does setup move: \(([A-K][1-5])\)} {
					set last_move "$expect_out(1,string)" }
				default {die}
			}
		} else {
			expect {
				-re {Dvonner does move: \(([A-K][1-5])-([A-K][1-5])\)} {
					# Reformat move in CodeCup format:
					set last_move "$expect_out(1,string)$expect_out(2,string)" }
				{Dvonner is passing} {set last_move {PASS}}
				{The game ended} {
					expect_user {
						Quit {quit}
						default {die}
					}
				}
				default {die}
			}
		}
		# Send move to user:
		msg "Sending Dvonner's move ($last_move) to user."
		send_user "$last_move\n"
	}

	# Update turn and next player:
	set turn [expr $turn + 1]
	if {$turn != 49} {
		if {$player eq {white}} {set player black} else {set player white}
	}
}
