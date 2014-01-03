#include "secd.h"
#include "memory.h"
#include "env.h"

#include <stdlib.h>

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

void run_secd(secd_t *secd, cell_t *ctrl) {
    cell_t *op;

    set_control(secd, ctrl);

    while (true)  {
        op = pop_control(secd);
        assertv(op, "run: no command");
        assert_or_continue(
                atom_type(op) == ATOM_FUNC,
                "run: not an opcode at [%ld]\n", cell_index(op));

        secd_opfunc_t callee = (secd_opfunc_t) op->as.atom.as.ptr;
        if (NULL == callee) return;  // STOP

        cell_t *ret = callee(secd);
        assertv(ret, "run: Instruction failed\n");
        drop_cell(op);
        ++secd->tick;
    }
}

