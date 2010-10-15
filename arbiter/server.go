package main

import (
	"bytes"
	"dvonn"
	"flag"
	"fmt"
	"net"
	"os"
	"path"
	"strings"
	"time"
)

const (
	OPEN  = iota
	LINE  = iota
	CLOSE = iota
)

type input struct {
	conn  *net.TCPConn
	event int
	line  string
}

type game struct {
	players    [2]*net.TCPConn
	state      dvonn.State
	begin, end time.Time
	result     string
}

var waiting *net.TCPConn = nil
var games = make(map[*net.TCPConn]*game)
var names = [2]string{"Player 1", "Player 2"}
var logDir = ""

func BeginGame(player1, player2 *net.TCPConn) *game {
	g := new(game)
	g.begin = *time.LocalTime()
	g.state.Init()
	g.players[0] = player1
	g.players[1] = player2
	games[player1] = g
	games[player2] = g
	player1.Write([]byte("Start\n"))
	return g
}

func WriteGameLog(g *game) {
	if logDir == "" {
		return
	}
	begin := g.begin.Format(time.RFC3339)
	end := g.end.Format(time.RFC3339)
	player1 := g.players[0].RemoteAddr().String()
	player2 := g.players[1].RemoteAddr().String()
	fileName := begin + "-" + player1 + "-" + player2 + ".txt"
	filePath := path.Join(logDir, fileName)
	w, err := os.Open(filePath, os.O_WRONLY|os.O_CREAT|os.O_EXCL, 0666)
	if err != nil {
		fmt.Println(err)
		return
	}
	fmt.Fprintln(w, "# Game begun at:", begin)
	fmt.Fprintln(w, "# Game ended at:", end)
	fmt.Fprintln(w, "# Player 1:", player1)
	fmt.Fprintln(w, "# Player 2:", player2)
	/*
		for it := range(g.state.moves.Iter()) {
			fmt.Fprintln(w, it.(fmt.Stringer))
		}
	*/
	line, parts := "", 0
	for elem := g.state.Moves.Front(); elem != nil; elem = elem.Next() {
		if line != "" {
			line += " "
		}
		line += elem.Value.(fmt.Stringer).String()
		parts++
		if parts == 49/2 || parts == 49 || (parts > 49 && (parts-49)%16 == 0) {
			fmt.Fprintln(w, line)
			line = ""
		}
	}
	if line != "" {
		fmt.Fprintln(w, line)
	}
	fmt.Fprintln(w, "#", g.result)
	w.Close()
	fmt.Println("Log written to", filePath)
}

func EndGame(g *game, result string) {
	g.end = *time.LocalTime()
	g.result = result
	for _, conn := range g.players {
		conn.Write([]byte("Quit\n"))
		conn.Write([]byte(result + "\n"))
	}
	if g.state.Moves.Len() > 0 {
		WriteGameLog(g)
	}
	games[g.players[0]] = nil
	games[g.players[1]] = nil
}

func getPlayerName(conn *net.TCPConn) string {
	if g := games[conn]; g != nil {
		for i, c := range g.players {
			if c == conn {
				return names[i]
			}
		}
	}
	return ""
}

func getOther(g *game, conn *net.TCPConn) *net.TCPConn {
	if conn == g.players[0] {
		return g.players[1]
	}
	if conn == g.players[1] {
		return g.players[0]
	}
	return nil
}

func handleInput(ch <-chan input) {
	for {
		in := <-ch
		switch in.event {
		case OPEN:
			if waiting == nil {
				waiting = in.conn
			} else {
				BeginGame(waiting, in.conn)
				waiting = nil
			}
		case CLOSE:
			if g := games[in.conn]; g != nil {
				EndGame(g, getPlayerName(in.conn)+" disconnected.")
			}
			if in.conn == waiting {
				waiting = nil
			}
			in.conn.Close()
		case LINE:
			if g := games[in.conn]; g == nil {
				in.conn.Write([]byte("The game is not in progress!\n"))
			} else if g.state.Player < 0 || g.players[g.state.Player] != in.conn {
				in.conn.Write([]byte("It's not your turn!\n"))
			} else {
				var error string
				switch g.state.Phase {
				case dvonn.PLACEMENT:
					if p, ok := dvonn.ParsePlace(in.line); ok {
						if !g.state.Place(p) {
							error = getPlayerName(in.conn) + " made an invalid placement: " + in.line
						}
					} else {
						error = getPlayerName(in.conn) + " sent an invalid command: " + in.line
					}
				case dvonn.MOVEMENT:
					if in.line == "PASS" {
						if !g.state.Pass() {
							error = getPlayerName(in.conn) + " cannot pass now"
						}
					} else if p, q, ok := dvonn.ParseMove(in.line); ok {
						if !g.state.Move(p, q) {
							error = getPlayerName(in.conn) + " made an invalid move: " + in.line
						}
					} else {
						error = getPlayerName(in.conn) + " sent an invalid command: " + in.line
					}
				}
				if error != "" {
					EndGame(g, error)
				} else {
					if g.state.Phase != dvonn.COMPLETE {
						getOther(g, in.conn).Write([]byte(in.line + "\n"))
					} else {
						a, b := g.state.Scores()
						reason := fmt.Sprintf("Game complete. Score: %d - %d. ", a, b)
						if a > b {
							reason += names[0] + " won!"
						} else if b > a {
							reason += names[1] + " won!"
						} else {
							reason += "It's a tie!"
						}
						EndGame(g, reason)
					}
				}
			}
		}
	}
}

func handleConnection(conn *net.TCPConn, ch chan<- input) {
	fmt.Println("Accepted connection from", conn.RemoteAddr())
	ch <- input{conn, OPEN, ""}
	buf, pos := [100]byte{}, 0
	for {
		if pos == len(buf) {
			fmt.Println("Received an overlong line from", conn.RemoteAddr())
			break
		}
		n, err := conn.Read(buf[pos:])
		if n > 0 {
			pos += n
			for {
				if i := bytes.IndexByte(buf[0:pos], '\n'); i < 0 {
					break
				} else {
					line := strings.TrimSpace(string(buf[0 : i+1]))
					ch <- input{conn, LINE, line}
					copy(buf[0:], buf[i+1:])
					pos -= i + 1
				}
			}
		} else {
			if err != os.EOF {
				fmt.Println(err)
			}
			break
		}
	}
	fmt.Println("Closing connection to", conn.RemoteAddr())
	ch <- input{conn, CLOSE, ""}
	// closing here would invalidate conn.RemoteAddr() so don't do that!
}

func main() {
	port := flag.Int("port", 2727, "TCP port to bind")
	flag.StringVar(&logDir, "logDir", "", "directory to write log files to")
	flag.Parse()
	if lsr, err := net.ListenTCP("tcp", &net.TCPAddr{nil, *port}); lsr == nil {
		fmt.Println(err)
	} else {
		fmt.Println("Server listening on port", *port)
		ch := make(chan input)
		go handleInput(ch)
		for {
			if conn, err := lsr.AcceptTCP(); conn == nil {
				fmt.Println(err)
				break
			} else {
				// CHECKME: does this really make writes non-blocking?
				conn.SetWriteTimeout(0)
				go handleConnection(conn, ch)
			}
		}
	}
}
