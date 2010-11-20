CFLAGS=-g -O0 -m32 -Wall -Wextra -DxTT_DEBUG -DZOBRIST
LDFLAGS=-m32
LDLIBS=-lm
SRCS=AI.c Crash.c Eval.c Game.c Game-steps.c IO.c MO.c Time.c TT.c player.c
OBJS=AI.o Crash.o Eval.o Game.o Game-steps.o IO.o MO.o Time.o TT.o player.o

# To compile with mudflap array/pointer verification:
#CFLAGS+=-fmudflap
#LDFLAGS+=-fmudflap -lmudflap

all: player submission.c

player: $(OBJS)
	$(CC) $(LDFLAGS) -o player $(OBJS) $(LDLIBS)

submission.c: tools/compile.pl $(SRCS)
	tools/compile.pl -DZOBRIST $(SRCS) >submission.c

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f player submission.c

.PHONY: all clean distclean
