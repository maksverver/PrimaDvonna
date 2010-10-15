This is an arbiter for Dvonn as played during the CodeCup. It consits of two
programs. One is a server program that binds a TCP socket, accepts two client
connections, and then lets those clients play against each other. The protocol
used is exactly equal to the CodeCup protocol, except that the game result is
written after the final "Quit" line from client to server. At the end of each
game the entire game is written to a logfile (if a logging directory has been
set). The server supports multiple games being played concurrently.

The second program is a local benchmark program, which takes various players,
and runs one or more full competitions between these players (every player
plays every other player twice; once for both colors). The results of this
tournament are then output. Contrary to the game server, the arbiter starts
clients locally, so it takes a shell command for each player on the command
line.

Neither the server nor the arbiter enforce time and memory limits or any other
restrictions. This allows easier testing with clients that were not written to
comply with the CodeCup rules. Both programs do verify that all programs follow
the rules (i.e. play only valid moves).
