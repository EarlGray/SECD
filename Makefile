VM      := ./secd
REPL    := repl.secd
SECDCC  := scm2secd.secd
CFLAGS  += -Os -Wall

BUILD_DIR := build
SRC_DIR   := vm

objs    := interp.o machine.o env.o memory.o native.o readparse.o ports.o
objs    := $(addprefix $(BUILD_DIR)/,$(objs))

# posix:
posixobjs   := $(addprefix $(BUILD_DIR)/,secd.o)
posixobjs   += $(objs)

.PHONY: clean libsecd
.PHONY: install uninstall

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

install:
	@mkdir -p $(INSTALL_DIR)/bin $(INSTALL_DIR)/share/secdscheme/{secd,std}
	@cp $(VM) secdscheme $(INSTALL_DIR)/bin/
	@cp $(REPL) $(SECDCC) $(INSTALL_DIR)/share/secdscheme/secd/
	@cp repl.scm scm2secd.scm std/* $(INSTALL_DIR)/share/secdscheme/std/

uninstall:
	@test "$(INSTALL_DIR)"
	@rm -r $(INSTALL_DIR)/bin/{secd,secdscheme} && rmdir $(INSTALL_DIR)/bin || true
	@rm -r $(INSTALL_DIR)/share/secdscheme/ && rmdir -p $(INSTALL_DIR)/share/secdscheme || true

clean:
	@echo "  rm *.o"
	@rm -r $(BUILD_DIR) || true
	@rm secd *.o 2>/dev/null || true
	@echo "  rm libsecd*"
	@rm libsecd* 2>/dev/null || true


