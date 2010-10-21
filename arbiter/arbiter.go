package main

import (
	"bufio"
	"dvonn"
	"exec"
	"flag"
	"fmt"
	"os"
	"rand"
	"sort"
	"strings"
	"time"
)

var logPath = ""
var showPlayerMessages = false

type Result struct {
	player [2]int   // 0-based player indices
	score  [2]int   // final score (number of discs)
	failed [2]bool  // whether player failed
	points [2]int   // CodeCup-style points
	time   [2]float // total time taken
}

type IntPair struct {
	first, second int
}

type IntPairSlice []IntPair

// Functions needed to satisfy sort.Interface:
func (ips IntPairSlice) Len() int {
	return len(ips)
}
func (ips IntPairSlice) Less(i, j int) bool {
	return ips[i].first < ips[j].first ||
		(ips[i].first == ips[j].first && ips[i].second < ips[j].second)
}
func (ips IntPairSlice) Swap(i, j int) {
	ips[i], ips[j] = ips[j], ips[i]
}

func (ips IntPairSlice) Reverse() {
	for i, j := 0, len(ips)-1; i < j; i, j = i+1, j-1 {
		ips.Swap(i, j)
	}
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
	return nil, nil // should never get here
}

func runMatch(players [2]int, commands [2]string, logPath string) Result {
	result := Result{player: players}

	var cmds [2]*exec.Cmd
	var reader [2]*bufio.Reader

	for i := range players {
		command := commands[i]
		if cmd, err := runPlayer(command); err != nil {
			fmt.Fprintf(os.Stderr, "Couldn't run '%s': %s\n", command, err.String())
			result.failed[i] = true
		} else {
			cmds[i] = cmd
			reader[i] = bufio.NewReader(cmd.Stdout)
			if i == 0 {
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
			line, err := reader[p].ReadString('\n')
			result.time[p] += float(time.Nanoseconds()-timeStart) / 1e9
			if err != nil {
				fmt.Fprintf(os.Stderr, "Failed to read from '%s': %s\n", commands[p], err.String())
				result.failed[p] = true
			} else {
				line = line[0 : len(line)-1] // discard trailing newline
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
		if moveStr != "" && !result.failed[1-p] && game.Phase != dvonn.COMPLETE {
			fmt.Fprintln(cmds[1-p].Stdin, moveStr)
		}
	}

	// Tell players to quit:
	for _, cmd := range cmds {
		if cmd != nil {
			if f := cmd.Stdin; f != nil {
				fmt.Fprintln(f, "Quit")
				f.Close()
			}
		}
	}

	// Wait for processes to quit:
	for _, cmd := range cmds {
		if cmd != nil {
			cmd.Close()
		}
	}

	// Determine scores:
	result.score[0], result.score[1] = game.Scores()

	// Determine competition points:
	for i := range players {
		if !result.failed[i] {
			result.points[i] = result.score[i]
			if result.score[i] > result.score[1-i] {
				result.points[i] += 90
			} else if result.score[i] == result.score[1-i] {
				result.points[i] += 45
			}
		}
	}

	// Write to log file, if desired:
	if logPath != "" {
		w, err := os.Open(logPath, os.O_WRONLY|os.O_CREAT|os.O_TRUNC, 0666)
		if err != nil {
			fmt.Println(err)
		} else {
			for i := range(players) {
				fmt.Fprintf(w, "# Player %d: %s\n", i + 1, commands[i])
			}
			game.WriteLog(w)
			for i := range(players) {
				if result.failed[i] {
					fmt.Fprintf(w, "# Player %d failed!\n", i + 1)
				}
			}
			summary := fmt.Sprintf("# Score: %d - %d. Time: %.3fs - %.3fs. ",
				result.score[0], result.score[1],
				result.time[0], result.time[1])
			if result.score[0] > result.score[1] {
				summary += "Player 1 won!"
			} else if result.score[1] > result.score[0] {
				summary += "Player 2 won!"
			} else {
				summary += "It's a tie!"
			}
			fmt.Fprintln(w, summary)
			w.Close()
		}
	}

	return result
}

func toYesNo(v bool) string {
	if v {
		return "yes"
	}
	return "no"
}

func runTournament(commands []string, rounds int, firstOnly bool) []Result {
	fmt.Printf(" Id        Player 1             Player 2        Score   Points  Failed       Time used\n")
	fmt.Printf("---- -------------------- --------------------  -----  -------  -------  -----------------\n")

	numResults := rounds * len(commands) * (len(commands) - 1)
	if firstOnly {
		numResults = 1
	}
	results := make([]Result, numResults)
	n := 0
outermost:
	for r := 0; r < rounds; r++ {
		for i := range commands {
			for j := range commands {
				if i != j {
					logFilePath := ""
					if logPath != "" {
						logFilePath = fmt.Sprintf("%s%04d.log", logPath, n + 1)
					}
					res := runMatch([2]int{i, j}, [2]string{commands[i], commands[j]}, logFilePath)
					player1 := shorten(commands[i], 20)
					player2 := shorten(commands[j], 20)
					if res.score[0] > res.score[1] {
						player1 = strings.ToUpper(player1)
					} else if res.score[1] > res.score[0] {
						player2 = strings.ToUpper(player2)
					}
					fmt.Printf(
						"%4d %-20s %-20s  %2d %2d  %3d %3d  %-3s %-3s  %7.3fs %7.3fs\n",
						n + 1, player1, player2,
						res.score[0], res.score[1],
						res.points[0], res.points[1],
						toYesNo(res.failed[0]), toYesNo(res.failed[1]),
						res.time[0], res.time[1])
					results[n] = res
					n++
					if firstOnly {
						break outermost
					}
				}
			}
		}
	}
	fmt.Printf("---- -------------------- --------------------  -----  -------  -------  -----------------\n")
	return results
}

func shorten(in string, n int) string {
	if len(in) <= n {
		return in
	}
	if n < 5 {
		return in[0:n]
	}
	a, b := (n-2)/2, (n-2)-(n-2)/2
	return in[0:a] + ".." + in[len(in)-b:]
}

func main() {
	rand.Seed(time.Nanoseconds())
	rounds := 1
	single := false
	flag.IntVar(&rounds, "rounds", rounds, "number of rounds to play")
	flag.BoolVar(&showPlayerMessages, "messages", showPlayerMessages, "pass player messages through")
	flag.BoolVar(&single, "single", single, "play only a single game")
	flag.StringVar(&logPath, "log", logPath, "path to log files")
	flag.Parse()
	if flag.NArg() < 2 {
		fmt.Fprintln(os.Stderr, "Too few player commands passed!")
		fmt.Fprintln(os.Stderr, "Additional options:")
		flag.PrintDefaults()
	} else if rounds < 1 {
		fmt.Fprintln(os.Stderr, "Invalid number of rounds passed!")
	} else if single && (flag.NArg() > 2 || rounds > 1) {
		fmt.Fprintln(os.Stderr, "Single game requires two players and one round!")
	} else {
		players := flag.Args()
		results := runTournament(players, rounds, single)
		numGames := rounds * (len(players) - 1) * 2 // per player
		if single {
			numGames = 1
		}

		// Collect some game statistics:
		totalPoints := make([]int, len(players))
		gamesWon := make([]int, len(players))
		gamesTied := make([]int, len(players))
		gamesLost := make([]int, len(players))
		gamesFailed := make([]int, len(players))
		timeUsed := make([]float, len(players))
		timeMax := make([]float, len(players))
		for _, result := range results {
			for i := 0; i < 2; i++ {
				player := result.player[i]
				totalPoints[player] += result.points[i]
				if result.failed[i] {
					gamesFailed[player]++
				}
				if result.score[i] > result.score[1-i] {
					gamesWon[player]++
				}
				if result.score[i] == result.score[1-i] {
					gamesTied[player]++
				}
				if result.score[i] < result.score[1-i] {
					gamesLost[player]++
				}
				timeUsed[player] += result.time[i]
				if result.time[i] > timeMax[player] {
					timeMax[player] = result.time[i]
				}
			}
		}

		// Sort players by total points:
		pointsPlayers := make(IntPairSlice, len(players))
		for i := range pointsPlayers {
			pointsPlayers[i] = IntPair{totalPoints[i], -i}
		}
		sort.Sort(pointsPlayers)
		pointsPlayers.Reverse()

		// Print ranking ordered by Codecup total game points
		fmt.Println()
		fmt.Println("No Player               Points  Won Tied Lost Fail Avg Time Max Time")
		fmt.Println("-- -------------------- ------ ---- ---- ---- ---- -------- --------")
		for i, ip := range pointsPlayers {
			p := -ip.second
			fmt.Printf("%2d %-20s %6d %4d %4d %4d %4d %7.3fs %7.3fs\n",
				i+1, shorten(players[p], 20), totalPoints[p], gamesWon[p], gamesTied[p], gamesLost[p],
				gamesFailed[p], timeUsed[p]/float(numGames), timeMax[p])
		}
		fmt.Println("-- -------------------- ------ ---- ---- ---- ---- -------- --------")

		// - print win/draw/loss matrix
		// - print average disk difference matrix
	}
}
