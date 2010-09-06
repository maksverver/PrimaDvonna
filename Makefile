CFLAGS=-g -O2 -Wall -Wextra
LDFLAGS=
OBJS=AI.o Game.o player.o

# To compile with mudflap array/pointer verification:
#CFLAGS+=-fmudflap
#LDFLAGS+=-fmudflap -lmudflap

all: player

player: $(OBJS)
	$(CC) $(LDFLAGS) -o player $(OBJS) $(LDLIBS)

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f player

.PHONY: all clean distclean
