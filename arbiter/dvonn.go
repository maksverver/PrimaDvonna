package dvonn

import "container/list"

const (
	PLACEMENT = iota
	MOVEMENT  = iota
	COMPLETE  = iota
)

const (
	H = 5
	W = 11
)

type Field struct {
	Pieces int  // number of pieces on this field
	Player int  // owner of the pieces on this field (-1, 0 or 1)
	Dvonn  bool // whether this field contains a dvonn piece
}

var EmptyField = Field{0, -1, false}

type State struct {
	Player int // who plays next?
	Phase  int
	Moves  list.List
	Fields [49]Field
}

type Place struct {
	P Coords
}

type Move struct {
	P, Q Coords
}

type Pass struct{}

type Coords struct {
	X, Y int
}

var dx = [6]int{-1, -1, 0, 0, +1, +1}
var dy = [6]int{-1, 0, -1, +1, 0, +1}

func (s *State) Init() {
	s.Player = 0
	s.Phase = PLACEMENT
	s.Moves.Init()
	for _, f := range s.Fields {
		f.Pieces = 0
		f.Player = -1
		f.Dvonn = false
	}
}

// Parses the given string as a pair of coordinates (e.g. "A1" -> {0,0})
func ParseCoords(s string) (Coords, bool) {
	if len(s) == 2 && s[0] >= 'A' && s[0] <= 'K' && s[1] >= '1' && s[1] <= '5' {
		return Coords{int(s[0] - 'A'), int(s[1] - '1')}, true
	}
	return Coords{}, false
}

func ParsePlace(s string) (Coords, bool) {
	return ParseCoords(s)
}

func ParseMove(s string) (p Coords, q Coords, ok bool) {
	if len(s) == 4 {
		if p, ok = ParseCoords(s[0:2]); ok {
			if q, ok = ParseCoords(s[2:4]); ok {
				return
			}
		}
	}
	return Coords{}, Coords{}, false
}

func FormatCoords(c Coords) string {
	return string([]byte{byte('A' + c.X), byte('1' + c.Y)})
}

func (p Pass) String() string {
	return "PASS"
}

func (p Place) String() string {
	return FormatCoords(p.P)
}

func (m Move) String() string {
	return FormatCoords(m.P) + FormatCoords(m.Q)
}

// Converts field coordinates into an index between 0 and 49 (exclusive),
// or returns -1 if the given coordinates do not designate a valid field.
func CoordsIndex(p Coords) int {
	var begin = [5]int{0, 0, 0, 1, 2}
	var end = [5]int{9, 10, 11, 11, 11}
	var offset = [5]int{0, 9, 19, 29, 38}
	if p.Y >= 0 && p.Y < 5 && p.X >= begin[p.Y] && p.X < end[p.Y] {
		return p.X + offset[p.Y]
	}
	return -1
}

func abs(i int) int {
	if i < 0 {
		return -i
	}
	return i
}

func max(i, j int) int {
	if i < j {
		return j
	}
	return i
}

// Returns the distance between a pair of coordinates.
func distance(p, q Coords) int {
	dx := q.X - p.X
	dy := q.Y - p.Y
	dz := dx - dy
	return max(max(abs(dx), abs(dy)), abs(dz))
}

// Returns whether p and q are colinear along one of the six board axes.
func colinear(p, q Coords) bool {
	dx, dy := q.X-p.X, q.Y-p.Y
	return dx == 0 || dy == 0 || dx == dy
}

// Returns whether the given field has fewer than six occupied neighbours.
func mobile(s *State, p Coords) bool {
	for d, _ := range dx {
		i := CoordsIndex(Coords{p.X + dx[d], p.Y + dy[d]})
		if i < 0 || s.Fields[i].Pieces == 0 {
			return true
		}
	}
	return false
}

// Returns whether a stone can be placed on the given point:
func (s *State) CanPlace(p Coords) bool {
	i := CoordsIndex(p)
	return s.Phase == PLACEMENT && i >= 0 && s.Fields[i].Pieces == 0
}

// Places the next piece on the given point and returns true, or returns false
// if the given move is invalid.
func (s *State) Place(p Coords) bool {
	if !s.CanPlace(p) {
		return false
	}
	i := CoordsIndex(p)
	if s.Moves.Len() < 3 {
		s.Fields[i] = Field{1, -1, true}
	} else {
		s.Fields[i] = Field{1, s.Player, false}
	}
	s.Moves.PushBack(Place{p})
	if s.Moves.Len() < 49 {
		s.Player = 1 - s.Player
	} else {
		s.Phase = MOVEMENT
	}
	return true
}

// Determines whether the given player has any valid moves.
func (s *State) canPlay(player int) bool {
	for x1 := 0; x1 < W; x1++ {
		for y1 := 0; y1 < H; y1++ {
			for x2 := 0; x2 < W; x2++ {
				for y2 := 0; y2 < H; y2++ {
					if s.canPlayerMove(player, Coords{x1, y1}, Coords{x2, y2}) {
						return true
					}
				}
			}
		}
	}
	return false
}

// Returns whether the given move is valid.
func (s *State) canPlayerMove(player int, p, q Coords) bool {
	i, j := CoordsIndex(p), CoordsIndex(q)
	return s.Phase == MOVEMENT &&
		i >= 0 && s.Fields[i].Pieces > 0 &&
		j >= 0 && s.Fields[j].Pieces > 0 &&
		s.Fields[i].Player == player && mobile(s, p) &&
		colinear(p, q) && distance(p, q) == s.Fields[i].Pieces
}

// Returns whether the current player must pass.
func (s *State) CanPass() bool {
	return !s.canPlay(s.Player)
}

// Passes the current players turn and returns true, or returns false if
// the player is not allowed to pass because he has valid moves to play.
func (s *State) Pass() bool {
	if !s.CanPass() {
		return false
	}
	s.Player = 1 - s.Player
	s.Moves.PushBack(Pass{})
	return true
}

// Returns whether the given move is valid for the current player.
func (s *State) CanMove(p, q Coords) bool {
	return s.canPlayerMove(s.Player, p, q)
}

// Marks the field i (with corresponding coordinates (x, y)) as reachable,
// and then calls the function recursively for all unreachable adjacent fields
// with pieces on them.
func (s *State) markReachable(reachable *[49]bool, i, x, y int) {
	reachable[i] = true
	for d, _ := range dx {
		nx, ny := x+dx[d], y+dy[d]
		j := CoordsIndex(Coords{nx, ny})
		if j >= 0 && s.Fields[j].Pieces > 0 && !reachable[j] {
			s.markReachable(reachable, j, nx, ny)
		}
	}
}

// Removes pieces which are unreachable from Dvonn pieces from the game:
func (s *State) removeUnreachable() {
	reachable := [49]bool{}
	for x := 0; x < W; x++ {
		for y := 0; y < H; y++ {
			i := CoordsIndex(Coords{x, y})
			if i >= 0 && s.Fields[i].Dvonn && !reachable[i] {
				s.markReachable(&reachable, i, x, y)
			}
		}
	}
	for i, r := range reachable {
		if !r && s.Fields[i].Pieces > 0 {
			s.Fields[i] = EmptyField
		}
	}
}

// Moves the stack from point p to point q and returns true, or returns false
// if the given move is invalid.
func (s *State) Move(p, q Coords) bool {
	if !s.CanMove(p, q) {
		return false
	}
	i, j := CoordsIndex(p), CoordsIndex(q)
	s.Fields[j].Pieces += s.Fields[i].Pieces
	s.Fields[j].Dvonn = s.Fields[j].Dvonn || s.Fields[i].Dvonn
	s.Fields[j].Player = s.Player
	s.Fields[i] = EmptyField
	s.removeUnreachable()
	if s.canPlay(0) || s.canPlay(1) {
		s.Player = 1 - s.Player
	} else {
		s.Phase = COMPLETE
		s.Player = -1
	}
	s.Moves.PushBack(Move{p, q})
	return true
}

// Returns the player's scores:
func (s *State) Scores() (int, int) {
	a, b := 0, 0
	for _, f := range s.Fields {
		switch f.Player {
		case 0:
			a += f.Pieces
		case 1:
			b += f.Pieces
		}
	}
	return a, b
}
