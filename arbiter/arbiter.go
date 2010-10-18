package main

import (
	// "dvonn"
	//"exec"
	"flag"
	"fmt"
	"os"
)

type Result struct {
	player  [2]int    // 0-based player indices
	discs   [2]int    // final number of discs
	failed  [2]bool   // whether player failed
	points  [2]int    // CodeCup-style points
	time    [2]float  // total time taken
}

func runMatch(commands []string, players [2]int) : result Result {
	result.player = players

	// TODO: fill in result.discs
	// TODO: fill in result.failed
	// TODO: fill in result.points
	// TODO: fill in result.time
}

func runTournament(commands []string, rounds int) : results []Result {
	/*
	for r := 0; r < rounds; r++ {
		fmt.Println("Round", r + 1, "of", rounds)
		for i, a := range(players) {
			for j, b := range(players) {
				if i != j {
					fmt.Println(a, "vs", b)
				}
			}
		}
	}
	*/
	results = []Result{runMatch(commands, int[2]{0, 1})}
}

func main() {
	rounds := 1
	flag.IntVar(&rounds, "rounds", rounds, "number of rounds to play")
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
