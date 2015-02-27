VM 		:= ./secd
REPL 	:= repl.secd
SECDCC 	:= scm2secd.secd
CFLAGS 	+= -O0 -g -Wall -Wextra

BUILD_DIR := build
SRC_DIR   := vm

objs 	:= interp.o machine.o env.o memory.o native.o readparse.o ports.o
objs    := $(addprefix $(BUILD_DIR)/,$(objs))

# posix:
posixobjs 	:= $(addprefix $(BUILD_DIR)/,secd.o)
posixobjs   += $(objs)

.PHONY: clean libsecd


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

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "  CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

TMP_C	:= $(BUILD_DIR)/temp.c
repl.o : repl.secd
	xxd -i $< > $(TMP_C)
	$(CC) $(CFLAGS) -c $(TMP_C) -o $@

%.secd: %.scm $(VM)
	@echo "  SECDCC $@"
	@$(VM) scm2secd.secd < $< > tmp.secd && mv tmp.secd $@

libsecd: libsecd.a

libsecd.a: $(objs) repl.o
	@echo "  AR libsecd.a"
	@$(AR) -r libsecd.a $(objs) repl.o

clean:
	@echo "  rm *.o"
	@rm -r $(BUILD_DIR) || true
	@rm secd *.o 2>/dev/null || true
	@echo "  rm libsecd*"
	@rm libsecd* 2>/dev/null || true


