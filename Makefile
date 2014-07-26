objs 	:= interp.o machine.o env.o memory.o native.o readparse.o

# posix:
objs 	+= posix-io.o secd.o

CFLAGS 	:= -O1 -g -Wall -Wextra
VM 		:= ./secd
SECDCC 	:= scm2secd.secd
REPL 	:= repl.secd

secdscheme: $(VM) $(REPL)

$(REPL): repl.scm

$(VM): $(objs)
	@echo "  LD $@"
	@$(CC) $(CFLAGS) $(objs) -o $@

.depend:
	@echo "  MKDEPEND"
	@$(CC) -MM *.h *.c > $@

sos: $(objs) sos.o repl.o
	$(CC) $(CFLAGS) $(objs) sos.o repl.o -o $@

%.o : %.c
	@echo "  CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o : %.secd
	@echo "LD  $<"
	@$(LD) -r -b binary -o $@ $<

%.secd: %.scm $(VM)
	@echo "  SECDCC $@"
	@$(VM) scm2secd.secd < $< > tmp.secd && mv tmp.secd $@

libsecd: $(objs) repl.o
	@echo "  AR libsecd.a"
	@ar -r libsecd.a $(objs) repl.o

.PHONY: clean
clean:
	@echo "  rm *.o"
	@rm secd *.o 2>/dev/null || true
	@echo "  rm libsecd*"
	@rm libsecd* 2>/dev/null || true

include .depend
