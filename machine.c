#include "secd.h"
#include "secd_io.h"
#include "memory.h"
#include "env.h"
#include "secdops.h"

#include <stdlib.h>

/*
 * SECD machine
 */

secd_t * init_secd(secd_t *secd) {
    /* allocate memory chunk */
    cell_t *heap = (cell_t *)calloc(N_CELLS, sizeof(cell_t));

    secd->free = SECD_NIL;
    secd->stack = secd->dump =
        secd->control = secd->env = SECD_NIL;

    init_mem(secd, heap, N_CELLS);

    secd->input_port = secd_stdin(secd);
    secd->output_port = secd_stdout(secd);

    init_env(secd);

    secd->tick = 0;
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
 *  Serialization
 */

cell_t *serialize_cell(secd_t *secd, cell_t *cell) {
    cell_t *opt;
    switch (cell_type(cell)) {
      case CELL_CONS: {
            cell_t *cdrc = new_cons(secd,
                    new_number(secd, cell_index(secd, get_cdr(cell))), SECD_NIL);
            cell_t *carc = new_cons(secd,
                    new_number(secd, cell_index(secd, get_car(cell))), cdrc);
            opt = new_cons(secd, new_symbol(secd, "cons"), carc);
        } break;
      case CELL_ERROR: {
            cell_t *msgc = new_cons(secd,
                    new_string(secd, errmsg(cell)), SECD_NIL);
            opt = new_cons(secd, new_symbol(secd, "err"), msgc);
        } break;
      case CELL_UNDEF:
        opt = new_cons(secd, new_symbol(secd, "#?"), SECD_NIL);
        break;
      case CELL_ARRAY: {
           cell_t *metac = new_cons(secd,
                   new_number(secd, cell_index(secd, arr_meta(cell->as.arr.data))),
                   SECD_NIL);
           opt = new_cons(secd, new_symbol(secd, "array"), metac);
        } break;
      case CELL_STR: {
           cell_t *metac = new_cons(secd,
                   new_number(secd, cell_index(secd, arr_meta((cell_t *)strmem(cell)))),
                   SECD_NIL);
           opt = new_cons(secd, new_symbol(secd, "str"), metac);
        } break;
      case CELL_PORT: {
           cell_t *portc;
           if (cell->as.port.file) {
               portc = new_cons(secd, new_symbol(secd, "file"), SECD_NIL);
           } else {
               cell_t *strc = new_cons(secd, 
                       new_number(secd, cell_index(secd, (cell_t*)strval(cell->as.port.as.str))), 
                       SECD_NIL);
               portc = new_cons(secd, new_symbol(secd, "str"), strc);
           }
           opt = new_cons(secd, new_symbol(secd, "port"), portc);
        } break;
      case CELL_ATOM: {
          switch (atom_type(secd, cell)) {
            case ATOM_INT: case ATOM_SYM: {
              cell_t *valc = new_cons(secd, cell, SECD_NIL);
              opt = new_cons(secd, new_symbol(secd, (atom_type(secd, cell) == ATOM_INT ? "int": "sym")), valc);
            } break;
            default: 
              opt = new_cons(secd, new_symbol(secd, "some_atom"), SECD_NIL);
          }
        } break;
      default: return SECD_NIL;
    }
    cell_t *refc = new_cons(secd, new_number(secd, cell->nref), opt);
    return new_cons(secd, new_number(secd, cell - secd->begin), refc);
}

cell_t *secd_mem_info(secd_t *secd) {
    cell_t *arrptr
        = new_cons(secd, new_number(secd, secd->arrayptr - secd->begin), SECD_NIL);
    cell_t *fxdptr
        = new_cons(secd, new_number(secd, secd->fixedptr - secd->begin), arrptr);
    cell_t *freec =
        new_cons(secd, new_number(secd, secd->free_cells), fxdptr);
    return new_cons(secd, new_number(secd, secd->end - secd->begin), freec);
}


/*
 * Errors
 */
cell_t secd_out_of_memory   = INIT_ERROR("Out of memory error");
cell_t secd_failure         = INIT_ERROR("General error");
cell_t secd_nil_failure     = INIT_ERROR("SECD_NIL error");

