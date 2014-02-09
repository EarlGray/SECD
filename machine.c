#include "secd.h"
#include "secd_io.h"
#include "memory.h"
#include "env.h"
#include "secdops.h"

#include <stdlib.h>
#include <sys/time.h>

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
    secd->debug_port = SECD_NIL;

    init_env(secd);

    secd->truth_value = lookup_env(secd, "#t", SECD_NIL);
    secd->false_value = SECD_NIL;

    secd->tick = 0;
    return secd;
}

cell_t * run_secd(secd_t *secd, cell_t *ctrl) {
    cell_t *op;

    set_control(secd, ctrl);

#if (TIMING)
    struct timeval ts_then;
    struct timeval ts_now;
#endif

    while (true)  {
#if (TIMING)
        gettimeofday(&ts_then, NULL);
#endif
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
        if (is_error(ret)) {
            errorf("run: %s failed at\n", symname(opcode_table[ opind ].sym));
            print_env(secd);
            return ret;
        }
        drop_cell(secd, op);

#if TIMING
        gettimeofday(&ts_now, NULL);
        int usec = ts_now.tv_usec - ts_then.tv_usec;
        if (usec < 0) usec += 1000000;
        ctrldebugf("    0.%06d s elapsed\n", usec);
#endif

        ++secd->tick;
    }
}

/*
 *  Serialization
 */
static cell_t *chain_index(secd_t *secd, const cell_t *cell, cell_t *prev) {
    return new_cons(secd, new_number(secd, cell_index(secd, cell)), prev);
}
static cell_t *chain_string(secd_t *secd, const char *str, cell_t *prev) {
    return new_cons(secd, new_string(secd, str), prev);
}
static cell_t *chain_sym(secd_t *secd, const char *str, cell_t *prev) {
    return new_cons(secd, new_symbol(secd, str), prev);
}

cell_t *serialize_cell(secd_t *secd, cell_t *cell) {
    cell_t *opt;
    switch (cell_type(cell)) {
      case CELL_CONS: {
            cell_t *cdrc = chain_index(secd, get_cdr(cell), SECD_NIL);
            opt = chain_index(secd, get_car(cell), cdrc);
        } break;
      case CELL_PORT: {
          if (cell->as.port.file) {
              opt = chain_sym(secd, "file", SECD_NIL);
          } else {
              cell_t *strc = chain_index(secd, (cell_t*)strval(cell->as.port.as.str), SECD_NIL);
              opt = chain_sym(secd, "str", strc);
          }
        } break;
      case CELL_ATOM:
          switch (atom_type(secd, cell)) {
            case ATOM_INT: case ATOM_SYM:
              opt = new_cons(secd, cell, SECD_NIL);
              break;
            case ATOM_OP: {
              cell_t *namec = new_const_clone(secd, opcode_table[ cell->as.atom.as.op ].sym);
              opt = new_cons(secd, namec, SECD_NIL);
            } break;
            case ATOM_FUNC:
              opt = new_cons(secd, new_number(secd, (long)cell->as.atom.as.ptr), SECD_NIL);
              break;
            default: opt = SECD_NIL;
          }
          break;
      case CELL_ARRMETA: {
            cell_t *meta = arr_ref(cell, -1);
            cell_t *arr = cell;
            if (!meta->as.mcons.cells) {
                arr = new_clone(secd, cell);
                arr->type = CELL_BYTES;
            }
            cell_t *arrc = new_cons(secd, arr, SECD_NIL);
            cell_t *nextc = chain_index(secd, mcons_next(meta), arrc);
            opt = chain_index(secd, mcons_prev(meta), nextc);
        } break;
      case CELL_FRAME: {
            cell_t *ioc = chain_index(secd, cell->as.frame.io, SECD_NIL);
            cell_t *nextc = chain_index(secd, cell->as.frame.cons.cdr, ioc);
            opt = chain_index(secd, cell->as.frame.cons.car, nextc);
        } break;
      case CELL_FREE: {
            cell_t *nextc = chain_index(secd, get_cdr(cell), SECD_NIL);
            opt = chain_index(secd, get_car(cell), nextc);
        } break;
      case CELL_REF: opt = chain_index(secd, cell->as.ref, SECD_NIL); break;
      case CELL_ERROR: opt = chain_string(secd, errmsg(cell), SECD_NIL); break;
      case CELL_UNDEF: opt = SECD_NIL; break;
      case CELL_ARRAY: opt = chain_index(secd, arr_val(cell, -1), SECD_NIL); break;
      case CELL_STR: opt = chain_index(secd, arr_meta((cell_t *)strmem(cell)), SECD_NIL); break;
      default: return SECD_NIL;
    }
    opt = new_cons(secd, secd_type_sym(secd, cell), opt);
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

