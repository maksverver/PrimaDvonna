package main

import (
	"bufio"
	"dvonn"
	"exec"
	"flag"
	"fmt"
	"os"
	"rand"
	"strings"
	"time"
)

var showPlayerMessages = false

type Result struct {
	player  [2]int    // 0-based player indices
	score   [2]int    // final score (number of discs)
	failed  [2]bool   // whether player failed
	points  [2]int    // CodeCup-style points
	time    [2]float  // total time taken
}

func runPlayer(command string) (*exec.Cmd, os.Error) {
	if argv := strings.Fields(command); len(argv) == 0 {
		return nil, os.EINVAL
	} else if name, err := exec.LookPath(argv[0]); err != nil {
		return nil, err
	} else if dir, err := os.Getwd(); err != nil {
		return nil, err
	} else {
		envv := os.Environ()
		stderr := exec.DevNull
		if showPlayerMessages {
			stderr = exec.PassThrough
		}
		return exec.Run(name, argv, envv, dir, exec.Pipe, exec.Pipe, stderr)
	}
	return nil, nil  // should never get here
}

func runMatch(players [2]int, commands [2]string) (result Result) {
	result.player = players

	var cmds [2]*exec.Cmd
	var reader [2]*bufio.Reader

	for i := range(players) {
		command := commands[i]
		if cmd, err := runPlayer(command); err != nil {
			fmt.Fprintf(os.Stderr, "Couldn't run '%s': %s\n", command, err.String())
			result.failed[i] = true
		} else {
			cmds[i] = cmd
			reader[i] = bufio.NewReader(cmd.Stdout)
			if i ==0 {
				// Send Start to first player
				fmt.Fprintln(cmd.Stdin, "Start")
			}
		}
	}

	var game dvonn.State
	game.Init()
	for game.Phase != dvonn.COMPLETE {
		moveStr := ""
		p := game.Player
		if result.failed[p] {
			// Player failed before; move randomly instead:
			moves := game.ListMoves()
			move := moves[rand.Intn(len(moves))]
			game.Execute(move)
			moveStr = move.(fmt.Stringer).String()
		} else {
			// Read move from client
			timeStart := time.Nanoseconds()
			if line, err := reader[p].ReadString('\n'); err != nil {
				fmt.Fprintf(os.Stderr, "Failed to read from '%s': %s\n", commands[p], err.String())
				result.failed[p] = true
			} else {
				result.time[p] += float(time.Nanoseconds() - timeStart)/1e9
				line = line[0:len(line)-1]  // discard trailing newline
				if move, ok := dvonn.Parse(line); !ok {
					fmt.Fprintf(os.Stderr, "Could not parse move from '%s': %s\n", commands[p], line)
					result.failed[p] = true
				} else if !game.Execute(move) {
					fmt.Fprintf(os.Stderr, "Invalid move from '%s': %s\n", commands[p], line)
					result.failed[p] = true
				} else {
					moveStr = move.(fmt.Stringer).String()
				}
			}
		}
		if moveStr != "" && cmds[1-p] != nil && cmds[1-p].Stdin != nil {
			fmt.Fprintln(cmds[1-p].Stdin, moveStr)
		}
	}

	// Tell players to quit:
	for _, cmd := range(cmds) {
		if cmd != nil {
			if f := cmd.Stdin; f != nil {
				fmt.Fprintln(f, "Quit")
				f.Close()
			}
		}
	}

	// Wait for processes to quit:
	for _, cmd := range(cmds) {
		if cmd != nil {
			cmd.Close()
		}
	}

	// Determine scores:
	result.score[0], result.score[1] = game.Scores()

	// Determine competition points:
	for i := range(players) {
		if !result.failed[i] {
			result.points[i] = result.score[i]
			if result.score[i] > result.score[1 - i] {
				result.points[i] += 90
			} else if result.score[i] == result.score[1 - i] {
				result.points[i] += 45
			}
		}
	}
	return
}

func toYesNo(v bool) string {
	if (v) {
		return "yes"
	}
	return "no"
}

func runTournament(commands []string, rounds int) []Result {
	fmt.Printf(" P1  P2  Score   Points   Failed      Time used\n")
	fmt.Printf(" --  --  -----  -------  -------  -----------------\n")
	results := make([]Result, rounds*len(commands)*(len(commands) - 1))
	n := 0
	for r := 0; r < rounds; r++ {
		for i := range(commands) {
			for j := range(commands) {
				if i != j {
					res := runMatch([2]int{i, j}, [2]string{commands[i], commands[j]})
					fmt.Printf(
						" %2d  %2d  %2d %2d  %3d %3d  %-3s %-3s  %7.3fs %7.3fs\n",
						res.player[0] + 1, res.player[1] + 1,
						res.score[0], res.score[1],
						res.points[0], res.points[1],
						toYesNo(res.failed[0]), toYesNo(res.failed[1]),
						res.time[0], res.time[1])
					results[n] = res
					n++
				}
			}
		}
	}
	fmt.Printf(" --  --  -----  -------  -------  -----------------\n")
	return results
}

func main() {
	rand.Seed(time.Nanoseconds())
	rounds := 1
	flag.IntVar(&rounds, "rounds", rounds, "number of rounds to play")
	flag.BoolVar(&showPlayerMessages, "messages", showPlayerMessages, "pass player messages through")
	flag.Parse()
	if flag.NArg() < 2 {
		fmt.Fprintln(os.Stderr, "Too few player commands passed!")
		fmt.Fprintln(os.Stderr, "Additional options:")
		flag.PrintDefaults()
	} else if rounds < 1 {
		fmt.Fprintln(os.Stderr, "Invalid number of rounds passed!")
	} else {
		players := flag.Args()
		runTournament(players, rounds)
		// tournament runs here.
		// - print ordered ranking with CodeCup total game points,
		//   games won, games tied, games lost, number of failures,
		//   average time used
		// - print win/draw/loss matrix
		// - print average disk difference matrix
	}
}
