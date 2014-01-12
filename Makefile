libobjs := interp.o machine.o env.o memory.o native.o readparse.o
objs := $(libobjs) secd.o

CFLAGS := -O2 -g -Wno-shift-overflow

secd: $(objs)
	$(CC) $(CFLAGS) $(objs) -o $@

repl.secd: repl.scm
	./secd scm2secd.secd < $< > $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ -Wall

libsecd: $(libobjs)
	ar -r libsecd.a $(libobjs)

.PHONY: clean
clean:
	rm secd *.o libsecd\* || true

interp.o : interp.c
machine.o : machine.c
env.o : env.c
native.o : native.c
memory.o : memory.c
readparse.o : readparse.c
secd.o : secd.c
