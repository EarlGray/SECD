#include "secd.h"
#include "secd_io.h"

#include <unistd.h>

int main(int argc, char *argv[]) {
    const char *mytty = NULL;
    if (isatty(STDIN_FILENO))
        mytty = ttyname(STDIN_FILENO);

    if (mytty) {
        errorf(";;;   Welcome to SECD   \n");
        errorf(";;;     sizeof(cell_t) is %zd\n", sizeof(cell_t));
        errorf(";;;     tty = %s\n", mytty);
        errorf(";;;   Type (secd) to get some help.\n");
    }

    secd_t secd;
    init_secd(&secd);

#if ((CTRLDEBUG) || (MEMDEBUG))
    secd_set_dbg(secd, secd_fopen(&secd, "secd.log", "w"));
#endif

    cell_t *cmdport = SECD_NIL;
    if (argc == 2) {
        cmdport = secd_fopen(&secd, argv[1], "r");
    }

    cell_t *inp = sexp_parse(&secd, cmdport); // cmdport is dropped after

    if (is_nil(inp) || !is_cons(inp)) {
        errorf("list of commands expected\n");
        dbg_printc(&secd, inp);
        return 1;
    }

    run_secd(&secd, inp);

    /* print the head of the stack */
    if (not_nil(secd.stack)) {
        envdebugf("Stack head:\n");
        dbg_printc(&secd, get_car(secd.stack));
    } else {
        envdebugf("Stack is empty\n");
    }
    return 0;
}
