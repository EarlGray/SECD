objs := interp.o machine.o env.o memory.o native.o readparse.o

CFLAGS := -O1 -g -Wno-shift-overflow -Wall -Wextra
VM := ./secd

$(VM): $(objs) secd.o
	$(CC) $(CFLAGS) $(objs) secd.o -o $@

sos: $(objs) sos.o repl.o
	$(CC) $(CFLAGS) $(objs) sos.o repl.o -o $@

repl.secd: repl.scm
	$(VM) scm2secd.secd < $< > $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o : %.secd
	$(LD) -r -b binary -o $@ $<

%.secd: %.scm $(VM)
	$(VM) scm2secd.secd < $< > tmp.secd && mv tmp.secd $@

libsecd: $(objs) repl.o
	ar -r libsecd.a $(objs) repl.o

.PHONY: clean
clean:
	rm secd *.o libsecd* || true

interp.o : interp.c memory.h secdops.h env.h
machine.o : machine.c memory.h secdops.h env.h
env.o : env.c memory.h env.h
native.o : native.c memory.h env.h
memory.o : memory.c memory.h
readparse.o : readparse.c memory.h secdops.h
secd.o : secd.c secd.h
sos.o: sos.c
memory.h : secd.h
secd.h: conf.h debug.h
repl.o: repl.secd
repl.secd: repl.scm
