package main

import (
	// "dvonn"
	"flag"
	"fmt"
	"os"
)

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
		// tournament runs here.
		// - print ordered ranking with CodeCup total game points,
		//   time used, and number of failures
		// - print win/draw/loss matrix
		// - print average disk difference matrix
	}
}
