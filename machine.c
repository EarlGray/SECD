#include "secd.h"
#include "secd_io.h"
#include "memory.h"
#include "env.h"
#include "secdops.h"

#include <stdlib.h>
#include <sys/time.h>

int secd_dump_state(secd_t *secd, const char *fname);

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

    secd->truth_value = share_cell(secd, new_symbol(secd, SECD_TRUE));
    secd->false_value = share_cell(secd, new_symbol(secd, SECD_FALSE));

    secd->input_port = share_cell(secd, secd_stdin(secd));
    secd->output_port = share_cell(secd, secd_stdout(secd));
    secd->debug_port = SECD_NIL;

    init_env(secd);

    secd->tick = 0;
    secd->postop = SECD_NOPOST;
    return secd;
}

static bool handle_exception(secd_t *secd, cell_t *exc) {
    return !is_error(secd_raise(secd, exc));
}

cell_t * run_secd(secd_t *secd, cell_t *ctrl) {
    cell_t *op;

    share_cell(secd, ctrl);
    set_control(secd, &ctrl);

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
                cell_type(op) == CELL_OP,
                "run: not an opcode at [%ld]\n", cell_index(secd, op));

        int opind = op->as.op;
        secd_opfunc_t callee = (secd_opfunc_t) opcode_table[ opind ].fun;
        if (SECD_NIL == callee)
            return SECD_NIL;  // STOP

        cell_t *ret = callee(secd);
        if (is_error(ret)) {
            if (!handle_exception(secd, ret)) {
                errorf("****************\n");
                print_env(secd);
                errorf("****************\n");
                errorf("FATAL EXCEPTION: %s failed\n", opcode_table[ opind ].name);
                return ret;
            }
        }
        drop_cell(secd, op);

#if TIMING
        gettimeofday(&ts_now, NULL);
        int usec = ts_now.tv_usec - ts_then.tv_usec;
        if (usec < 0) usec += 1000000;
        ctrldebugf("    0.%06d s elapsed\n", usec);
#endif
        switch (secd->postop) {
          case SECDPOST_GC:
              secd_mark_and_sweep_gc(secd);
              break;
          case SECDPOST_MACHINE_DUMP:
              secd_dump_state(secd, "secdstate.dump");
              break;
          case SECD_NOPOST:
              break;
        }
        secd->postop = SECD_NOPOST;

        ++secd->tick;
    }
}

/*
 *  Serialization
 */
const char * secd_type_names[] = {
    [CELL_UNDEF] = "void",
    [CELL_CONS]  = "cons",
    [CELL_ARRAY] = "vect",
    [CELL_STR]   = "str",
    [CELL_BYTES] = "bvect",
    [CELL_FRAME] = "frame",
    [CELL_KONT]  = "kont",
    [CELL_ARRMETA] = "meta",
    [CELL_FREE]  = "free",
    [CELL_REF]   = "ref",
    [CELL_SYM]   = "sym",
    [CELL_INT]   = "int",
    [CELL_CHAR]  = "char",
    [CELL_OP]    = "op",
    [CELL_FUNC]  = "func",
    [CELL_PORT]  = "port",
    [CELL_ERROR] = "err"
};

cell_t *secd_type_sym(secd_t *secd, const cell_t *cell) {
    const char *type = "unknown";
    enum cell_type t = cell_type(cell);
    assert(t <= CELL_ERROR, "secd_type_sym: type is invalid");
    type = secd_type_names[t];
    assert(type, "secd_type_names: unknown type of %d", t);
    return new_symbol(secd, type);
}

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
      case CELL_SYM:
          opt = new_cons(secd, cell, SECD_NIL);
          break;
      case CELL_INT: case CELL_CHAR:
        opt = new_cons(secd, cell, SECD_NIL);
        break;
      case CELL_OP: {
        cell_t *namec = new_symbol(secd, opcode_table[ cell->as.op ].name);
        opt = new_cons(secd, namec, SECD_NIL);
      } break;
      case CELL_FUNC:
        opt = new_cons(secd, new_number(secd, (long)cell->as.ptr), SECD_NIL);
        break;
      case CELL_ARRMETA: {
            cell_t *typec = chain_sym(secd,
                                      (cell->as.mcons.cells ? "cell" : "byte"),
                                      SECD_NIL);
            cell_t *nextc = chain_index(secd, mcons_next(cell), typec);
            opt = chain_index(secd, mcons_prev(cell), nextc);
        } break;
      case CELL_FRAME: {
            cell_t *ioc = chain_index(secd, cell->as.frame.io, SECD_NIL);
            cell_t *nextc = chain_index(secd, cell->as.frame.cons.cdr, ioc);
            opt = chain_index(secd, cell->as.frame.cons.car, nextc);
        } break;
      case CELL_KONT: {
            cell_t *kctrl = chain_index(secd, cell->as.kont.ctrl, SECD_NIL);
            cell_t *kenv  = chain_index(secd, cell->as.kont.env,  kctrl);
            opt = chain_index(secd, cell->as.kont.stack, kenv);
        } break;
      case CELL_FREE: {
            cell_t *nextc = chain_index(secd, get_cdr(cell), SECD_NIL);
            opt = chain_index(secd, get_car(cell), nextc);
        } break;
      case CELL_REF: opt = chain_index(secd, cell->as.ref, SECD_NIL); break;
      case CELL_ERROR: opt = chain_string(secd, errmsg(cell), SECD_NIL); break;
      case CELL_UNDEF: opt = SECD_NIL; break;
      case CELL_ARRAY:
        opt = chain_index(secd, arr_val(cell, -1), SECD_NIL);
        break;
      case CELL_STR: case CELL_BYTES:
        opt = chain_index(secd, arr_meta((cell_t *)strmem(cell)), SECD_NIL);
        break;
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
        new_cons(secd, new_number(secd, secd->stat.free_cells), fxdptr);
    return new_cons(secd, new_number(secd, secd->end - secd->begin), freec);
}

int secd_dump_state(secd_t *secd, const char *fname) {
    cell_t *p = secd_fopen(secd, fname, "w");
    secd_pprintf(secd, p,
            ";; secd->fixedptr = %ld\n", cell_index(secd, secd->fixedptr));
    secd_pprintf(secd, p,
            ";; secd->arrayptr = %ld\n", cell_index(secd, secd->arrayptr));
    secd_pprintf(secd, p,
            ";; secd->end      = %ld\n", cell_index(secd, secd->end));
    secd_pprintf(secd, p, ";; secd->input_port = %ld, secd->output_port = %ld\n",
            cell_index(secd, secd->input_port), cell_index(secd, secd->output_port));
    secd_pprintf(secd, p, ";; SECD = (%ld, %ld, %ld, %ld)\n",
            cell_index(secd, secd->stack), cell_index(secd, secd->env),
            cell_index(secd, secd->control), cell_index(secd, secd->dump));
    secd_pprintf(secd, p, ";; secd->free = %ld (%ld free)\n",
            cell_index(secd, secd->free), secd->stat.free_cells);
    /* dump fixed heap */
    long i;
    long n_fixed = secd->fixedptr - secd->begin;
    secd_pprintf(secd, p, "\n;; SECD persistent heap:\n");
    for (i = 0; i < n_fixed; ++i) {
        cell_t *cell_info = serialize_cell(secd, secd->begin + i);
        sexp_pprint(secd, p, cell_info);
        secd_pprintf(secd, p, "\n");
        free_cell(secd, cell_info);
    }

    secd_pprintf(secd, p, "\n;; SECD array heap:\n");
    cell_t *mcons = secd->arrlist;
    while (mcons_next(mcons)) {
        cell_t *cell_info = serialize_cell(secd, mcons);
        sexp_pprint(secd, p, cell_info);
        if (!mcons->as.mcons.free)
            secd_pdump_array(secd, p, mcons);
        secd_pprintf(secd, p, "\n");
        free_cell(secd, cell_info);

        mcons = mcons_next(mcons);
    }

    secd_pclose(secd, p);
    free_cell(secd, p);
    return 0;
}

