include $(GOROOT)/src/Make.inc

all: server arbiter

dvonn.$O: dvonn.go; $(GC) $<
dvonn.a: dvonn.$O; gopack grc $@ $<
server.$O: server.go dvonn.a; $(GC) -I. $<
server: server.$O; $(LD) -L. -o $@ $<
arbiter.$O: arbiter.go dvonn.a; $(GC) -I. $<
arbiter: arbiter.$O; $(LD) -L. -o $@ $<

clean: ; rm -f dvonn.$O dvonn.a server.$O arbiter.$O
distclean: clean; rm -f server arbiter

.PHONY: all clean distclean
