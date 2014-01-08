#include "secd.h"
#include "readparse.h"
#include "memory.h"

void print_env(secd_t *secd);

secd_t __attribute__((aligned(1 << SECD_ALIGN))) secd;

int main(int argc, char *argv[]) {
    init_secd(&secd);
    if (ENVDEBUG) print_env(&secd);

    FILE *op_in = NULL;
    if (argc == 2) {
        op_in = fopen(argv[1], "r");
    }

    envdebugf(">>>>>\n");
    cell_t *inp = read_secd(&secd, op_in);
    asserti(inp, "read_secd failed");
    if (is_nil(&secd, inp)) {
        printf("no commands.\n\n");
        return 0;
    }

    if (ENVDEBUG) {
        envdebugf("Control path:\n");
        sexp_print(&secd, inp);
    }
    envdebugf("<<<<<\n");

    run_secd(&secd, inp);

    envdebugf("-----\n");
    if (not_nil(&secd, secd.stack)) {
        envdebugf("Stack head:\n");
        printc(&secd, get_car(secd.stack));
    } else {
        envdebugf("Stack is empty\n");
    }
    return 0;
}
