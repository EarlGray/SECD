#include "secd.h"

#include <stdio.h>

int posix_getc(void *f)          { return fgetc(f); }
int stdin_getc(void __unused *f) { return getc(stdin); }

secd_stream_t posix_stdio = { .read = &posix_getc };
secd_stream_t posix_stdin = { .read = &stdin_getc };

secd_t __attribute__((aligned(1 << SECD_ALIGN))) secd;

int main(int argc, char *argv[]) {
    errorf(";;;   Welcome to SECD   \n");
    errorf(";;;     sizeof(cell_t) is %ld\n", sizeof(cell_t));
    errorf(";;;     Type (secd) to get some help.\n\n");

    init_secd(&secd, &posix_stdin);

    FILE *op_in = stdin;
    if (argc == 2) {
        op_in = fopen(argv[1], "r");
    }
    posix_stdio.state = op_in;

    cell_t *inp = read_secd(&secd, &posix_stdio);
    if (is_nil(inp)) {
        printf("no commands.\n\n");
        return 0;
    }
    if (is_error(inp)) { 
        errorf("read_secd failed: %s", errmsg(inp));
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
