VM      := ./secd
REPL    := repl.secd
SECDCC  := scm2secd.secd
CFLAGS  += -Wall -I./include

SRC_DIR   := vm

.PHONY: clean libsecd
.PHONY: install uninstall

secdscheme: $(VM) $(REPL)

$(REPL): repl.scm

$(VM): secd.o libsecd.a 
	$(CC) $(CFLAGS) $^ -o $@

.depend:
	@echo "  MKDEPEND"
	@$(CC) -MM *.h *.c > $@


%.secd: %.scm $(VM)
	@echo "  SECDCC $@"
	@$(VM) scm2secd.secd < $< > tmp.secd && mv tmp.secd $@

libsecd: libsecd.a

libsecd.a: libsecd.o
	$(AR) -r $@ $^

# the world simplest build system: cat!
libsecd.c:
	cat include/secd/conf.h include/secd/secd.h include/secd/secd_io.h >> $@
	cat vm/secdops.h vm/env.h vm/memory.h >> $@
	cat vm/*.c >> $@
	sed -i 's/^#include ".*"//' $@


install: $(VM) $(REPL)
	mkdir -p $(INSTALL_DIR)/bin $(INSTALL_DIR)/share/secdscheme/secd $(INSTALL_DIR)/share/secdscheme/std
	cp $(VM) $(REPL) $(SECDCC) secdscheme $(INSTALL_DIR)/share/secdscheme/secd/
	cp repl.scm scm2secd.scm std/* $(INSTALL_DIR)/share/secdscheme/std/
	echo "#!/bin/sh" > $(INSTALL_DIR)/bin/secdscheme
	echo 'exec $(INSTALL_DIR)/share/secdscheme/secd/secdscheme $$@' >> $(INSTALL_DIR)/bin/secdscheme
	chmod +x $(INSTALL_DIR)/bin/secdscheme

uninstall:
	test -d "$(INSTALL_DIR)"
	rm -r "$(INSTALL_DIR)/"

clean:
	@echo "  rm *.o"
	@rm secd *.o 2>/dev/null || true
	@echo "  rm libsecd*"
	@rm libsecd* 2>/dev/null || true


