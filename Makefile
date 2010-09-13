CFLAGS=-g -O2 -Wall -Wextra
LDFLAGS=
SRCS=AI.c Game.c IO.c player.c
OBJS=AI.o Game.o IO.o player.o

# To compile with mudflap array/pointer verification:
#CFLAGS+=-fmudflap
#LDFLAGS+=-fmudflap -lmudflap

all: player submission.c

player: $(OBJS)
	$(CC) $(LDFLAGS) -o player $(OBJS) $(LDLIBS)

submission.c: tools/compile.pl $(SRCS)
	tools/compile.pl $(SRCS) >submission.c

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f player submission.c

.PHONY: all clean distclean
