objs := interp.o machine.o env.o memory.o native.o readparse.o

# posix:
objs += posix-io.o secd.o

CFLAGS := -O2 -g -Wall -Wextra
VM := ./secd

$(VM): $(objs)
	$(CC) $(CFLAGS) $(objs) -o $@

.depend:
	$(CC) -MM *.h *.c > $@

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

include .depend
