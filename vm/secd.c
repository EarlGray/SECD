#include "secd.h"
#include "secd_io.h"

#include <unistd.h>
#include <stdlib.h>

#define N_CELLS     256 * 1024

int main(int argc, char *argv[]) {
    secd_t secd;
    cell_t *heap = (cell_t *)malloc(sizeof(cell_t) * N_CELLS);

    init_secd(&secd, heap, N_CELLS);
#if ((CTRLDEBUG) || (MEMDEBUG))
    secd_setport(&secd, SECD_STDDBG, secd_fopen(&secd, "secd.log", "w"));
#endif

    cell_t *cmdport = SECD_NIL;
    if (argc == 2)
        cmdport = secd_fopen(&secd, argv[1], "r");

    cell_t *inp = sexp_parse(&secd, cmdport); // cmdport is dropped after
    if (is_nil(inp) || !is_cons(inp)) {
        secd_errorf(&secd, "list of commands expected\n");
        dbg_printc(&secd, inp);
        return 1;
    }

    cell_t *ret;
    ret = run_secd(&secd, inp);

    return (is_error(ret) ? EXIT_FAILURE : EXIT_SUCCESS);
}
