#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>

void print_cell(const cell_t *c);
cell_t *free_cell(cell_t *c);

void print_env(secd_t *secd);
cell_t *lookup_env(secd_t *secd, const char *symname);
cell_t *sexp_parse(secd_t *secd, FILE *f);

void print_opcode(void *opptr);





/*
 * SECD machine
 */

secd_t * init_secd(secd_t *secd) {
    /* allocate memory chunk */
    secd->data = (cell_t *)calloc(N_CELLS, sizeof(cell_t));

    init_mem(secd, N_CELLS);

    secd->free = secd->data;
    secd->stack = secd->dump =  secd->nil;
    secd->control = secd->env =  secd->nil;

    secd->free_cells = N_CELLS - 1;
    secd->used_stack = 0;

    secd->tick = 0;

    init_env(secd);

    return secd;
}

void run_secd(secd_t *secd) {
    cell_t *op;
    while (true)  {
        op = pop_control(secd);
        assertv(op, "run: no command");
        assert_or_continue(
                atom_type(op) == ATOM_FUNC,
                "run: not an opcode at [%ld]\n", cell_index(op));

        secd_opfunc_t callee = (secd_opfunc_t) op->as.atom.as.op.fun;
        if (NULL == callee) return;  // STOP

        cell_t *ret = callee(secd);
        assertv(ret, "run: Instruction failed\n");
        drop_cell(op);
        ++secd->tick;
    }
}

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
    if (is_nil(inp)) {
        printf("no commands.\n\n");
        return 0;
    }

    set_control(&secd, inp);
    if (ENVDEBUG) {
        envdebugf("Control path:\n");
        print_list(secd.control);
    }
    envdebugf("<<<<<\n");

    run_secd(&secd);

    envdebugf("-----\n");
    if (not_nil(secd.stack)) {
        envdebugf("Stack head:\n");
        printc(get_car(secd.stack));
    } else {
        envdebugf("Stack is empty\n");
    }
    return 0;
}
