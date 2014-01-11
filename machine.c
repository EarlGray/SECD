#include "secd.h"
#include "memory.h"
#include "env.h"
#include "secdops.h"

#include <stdlib.h>

/*
 * SECD machine
 */

secd_t * init_secd(secd_t *secd, secd_stream_t *readstream) {
    /* allocate memory chunk */
    cell_t *heap = (cell_t *)calloc(N_CELLS, sizeof(cell_t));

    init_mem(secd, heap, N_CELLS);

    secd->free = SECD_NIL;
    secd->stack = secd->dump =
        secd->control = secd->env = SECD_NIL;

    secd->input = readstream;

    secd->used_stack = 0;
    secd->used_dump = 0;

    secd->tick = 0;

    init_env(secd);
    return secd;
}

cell_t * run_secd(secd_t *secd, cell_t *ctrl) {
    cell_t *op;

    set_control(secd, ctrl);

    while (true)  {
        op = pop_control(secd);
        assert_cell(op, "run: no command");
        assert_or_continue(
                atom_type(secd, op) == ATOM_OP,
                "run: not an opcode at [%ld]\n", cell_index(secd, op));

        int opind = op->as.atom.as.op;
        secd_opfunc_t callee = (secd_opfunc_t) opcode_table[ opind ].fun;
        if (SECD_NIL == callee)
            return SECD_NIL;  // STOP

        cell_t *ret = callee(secd);
        assert_cell(ret, "run: Instruction failed\n");
        drop_cell(secd, op);

        ++secd->tick;
    }
}


/*
 * Errors
 */
cell_t secd_out_of_memory   = INIT_ERROR("Out of memory error");
cell_t secd_failure         = INIT_ERROR("General error");
cell_t secd_nil_failure     = INIT_ERROR("SECD_NIL error");

