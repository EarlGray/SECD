VM 		:= ./secd
REPL 	:= repl.secd
SECDCC 	:= scm2secd.secd
CFLAGS 	+= -O0 -g -Wall -Wextra

objs 	:= interp.o machine.o env.o memory.o native.o readparse.o ports.o
# posix:
posixobjs 	:= $(objs) secd.o

secdscheme: $(VM) $(REPL)

$(REPL): repl.scm

$(VM): $(posixobjs)
	@echo "  LD $@"
	@$(CC) $(CFLAGS) $(posixobjs) -o $@

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
	@$(LD) $(LDFLAGS) -r -b binary -o $@ $<

%.secd: %.scm $(VM)
	@echo "  SECDCC $@"
	@$(VM) scm2secd.secd < $< > tmp.secd && mv tmp.secd $@

libsecd: $(objs) repl.o
	@echo "  AR libsecd.a"
	@$(AR) -r libsecd.a $(objs) repl.o

.PHONY: clean
clean:
	@echo "  rm *.o"
	@rm secd *.o 2>/dev/null || true
	@echo "  rm libsecd*"
	@rm libsecd* 2>/dev/null || true


