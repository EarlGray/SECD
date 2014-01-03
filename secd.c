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
 * Some native functions
 */


cell_t *secdf_list(secd_t __unused *secd, cell_t *args) {
    ctrldebugf("secdf_list\n");
    return args;
}

cell_t *secdf_null(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_nullp\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, is_nil(list_head(args)));
}

cell_t *secdf_nump(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_nump\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, atom_type(list_head(args)) == ATOM_INT);
}

cell_t *secdf_symp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_symp\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, atom_type(list_head(args)) == ATOM_SYM);
}

static cell_t *list_copy(secd_t *secd, cell_t *list, cell_t **out_tail) {
    if (is_nil(list))
        return secd->nil;

    cell_t *new_head, *new_tail;
    new_head = new_tail = new_cons(secd, list_head(list), secd->nil);

    while (not_nil(list = list_next(list))) {
        cell_t *new_cell = new_cons(secd, get_car(list), secd->nil);
        new_tail->as.cons.cdr = share_cell(new_cell);
        new_tail = list_next(new_tail);
    }
    if (out_tail)
        *out_tail = new_tail;
    return new_head;
}

cell_t *secdf_copy(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_copy\n");
    return list_copy(secd, list_head(args), NULL);
}

cell_t *secdf_append(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_append\n");
    assert(args, "secdf_append: args is NULL");

    if (is_nil(args))
        return args;

    cell_t *xs = list_head(args);
    assert(is_cons(list_next(args)), "secdf_append: expected two arguments");

    cell_t *argtail = list_next(args);
    if (is_nil(argtail))
        return xs;

    cell_t *ys = list_head(argtail);
    if (not_nil(list_next(argtail))) {
          ys = secdf_append(secd, argtail);
    }

    if (is_nil(xs))
        return ys;

    cell_t *sum = xs;
    cell_t *sum_tail = xs;
    while (true) {
        if (sum_tail->nref > 1) {
            sum_tail = NULL; // xs must be copied
            break;
        }
        if (is_nil(list_next(sum_tail)))
            break;
        sum_tail = list_next(sum_tail);
    }

    if (sum_tail) {
        ctrldebugf("secdf_append: destructive append\n");
        sum_tail->as.cons.cdr = share_cell(ys);
        sum = xs;
    } else {
        ctrldebugf("secdf_append: copying append\n");
        cell_t *sum_tail;
        sum = list_copy(secd, xs, &sum_tail);
        sum_tail->as.cons.cdr = share_cell(ys);
    }

    return sum;
}

cell_t *secdf_eofp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_eofp\n");
    cell_t *arg1 = list_head(args);
    if (atom_type(arg1) != ATOM_SYM)
        return secd->nil;
    return to_bool(secd, str_eq(symname(arg1), EOF_OBJ));
}

cell_t *secdf_ctl(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_ctl\n");
    if (is_nil(args))
        goto help;

    cell_t *arg1 = list_head(args);
    if (atom_type(arg1) == ATOM_SYM) {
        if (str_eq(symname(arg1), "free")) {
            printf("SECDCTL: Available cells: %lu\n", secd->free_cells);
        } else if (str_eq(symname(arg1), "env")) {
            print_env(secd);
        } else if (str_eq(symname(arg1), "tick")) {
            printf("SECDCTL: tick = %lu\n", secd->tick);
        } else {
            goto help;
        }
    }
    return args;
help:
    printf("SECDCTL: options are 'tick', 'env', 'free'\n");
    return args;
}

cell_t *secdf_getenv(secd_t *secd, cell_t __unused *args) {
    ctrldebugf("secdf_getenv\n");
    return secd->env;
}

cell_t *secdf_bind(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_bind\n");

    assert(not_nil(args), "secdf_bind: can't bind nothing to nothing");
    cell_t *sym = list_head(args);
    assert(atom_type(sym) == ATOM_SYM, "secdf_bind: a symbol must be bound");

    args = list_next(args);
    assert(not_nil(args), "secdf_bind: No value for binding");
    cell_t *val = list_head(args);

    cell_t *env;
    // is there the third argument?
    if (not_nil(list_next(args))) {
        args = list_next(args);
        env = list_head(args);
    } else {
        env = secd->global_env;
    }

    cell_t *frame = list_head(env);
    cell_t *old_syms = get_car(frame);
    cell_t *old_vals = get_cdr(frame);

    // an intersting side effect: since there's no check for
    // re-binding an existing symbol, we can create multiple
    // copies of it on the frame, the last added is found
    // during value lookup, but the old ones are persistent
    frame->as.cons.car = share_cell(new_cons(secd, sym, old_syms));
    frame->as.cons.cdr = share_cell(new_cons(secd, val, old_vals));

    drop_cell(old_syms); drop_cell(old_vals);
    return sym;
}

cell_t *compile_control_path(secd_t *secd, cell_t *control) {
    assert(control, "control path is NULL");
    cell_t *compiled = secd->nil;

    cell_t *cursor = control;
    cell_t *compcursor = compiled;
    while (not_nil(cursor)) {
        cell_t *opcode = list_head(cursor);
        if (atom_type(opcode) != ATOM_SYM) {
            errorf("compile_control: not a symbol in control path\n");
            sexp_print(opcode); printf("\n");
            return NULL;
        }

        index_t opind = search_opcode_table(opcode);
        assert(opind >= 0, "Opcode not found: %s", symname(opcode))

        cell_t *new_cmd = new_clone(secd, opcode_table[opind].val);
        cell_t *cc = new_cons(secd, new_cmd, secd->nil);
        if (not_nil(compcursor)) {
            compcursor->as.cons.cdr = share_cell(cc);
            compcursor = list_next(compcursor);
        } else {
            compiled = compcursor = cc;
        }
        cursor = list_next(cursor);

        cell_t *new_tail;

        if (new_cmd->as.atom.as.op.fun == &secd_ap) {
            cell_t *next = list_head(cursor);
            if (atom_type(next) == ATOM_INT) {
                new_tail = new_cons(secd, next, secd->nil);
                compcursor->as.cons.cdr = share_cell(new_tail);
                compcursor = list_next(compcursor);
                cursor = list_next(cursor);
            }
        }

        if (opcode_table[opind].args > 0) {
            if (new_cmd->as.atom.as.op.fun == &secd_sel) {
                cell_t *thenb = compile_control_path(secd, list_head(cursor));
                new_tail = new_cons(secd, thenb, secd->nil);
                compcursor->as.cons.cdr = share_cell(new_tail);
                compcursor = list_next(compcursor);
                cursor = list_next(cursor);

                cell_t *elseb = compile_control_path(secd, list_head(cursor));
                new_tail = new_cons(secd, elseb, secd->nil);
                compcursor->as.cons.cdr = share_cell(new_tail);
                compcursor = list_next(compcursor);
                cursor = list_next(cursor);
            } else {
                new_tail = new_cons(secd, list_head(cursor), secd->nil);
                compcursor->as.cons.cdr = share_cell(new_tail);
                compcursor = list_next(compcursor);
                cursor = list_next(cursor);
            }
        }
    }
    return compiled;
}

bool is_control_compiled(cell_t *control) {
    return atom_type(list_head(control)) == ATOM_FUNC;
}

#if TAILRECURSION
/*
 * returns a new dump for a tail-recursive call
 * or NULL if no Tail Call Optimization.
 */
static cell_t * new_dump_if_tailrec(cell_t *control, cell_t *dump) {
    if (is_nil(control))
        return NULL;

    cell_t *nextop = list_head(control);
    if (atom_type(nextop) != ATOM_FUNC)
        return NULL;
    secd_opfunc_t opfun = nextop->as.atom.as.op.fun;

    if (opfun == &secd_rtn) {
        return dump;
    } else if (opfun == &secd_join) {
        cell_t *join = list_head(dump);
        return new_dump_if_tailrec(join, list_next(dump));
    } else if (opfun == &secd_cons) {
        /* a situation of CONS CAR - it is how `begin` implemented */
        cell_t *nextcontrol = list_next(control);
        cell_t *afternext = list_head(nextcontrol);
        if (atom_type(afternext) != ATOM_FUNC)
            return NULL;
        if (afternext->as.atom.as.op.fun != &secd_car)
            return NULL;
        return new_dump_if_tailrec(list_next(nextcontrol), dump);
    }

    /* all other commands (except DUM, which must have RAP after it)
     * mess with the stack. TCO's not possible: */
    return NULL;
}
#endif

const cell_t list_sym   = INIT_SYM("list");
const cell_t append_sym = INIT_SYM("append");
const cell_t copy_sym   = INIT_SYM("copy");
const cell_t nullp_sym  = INIT_SYM("null?");
const cell_t nump_sym   = INIT_SYM("number?");
const cell_t symp_sym   = INIT_SYM("symbol?");
const cell_t eofp_sym   = INIT_SYM("eof-object?");
const cell_t debug_sym  = INIT_SYM("secdctl");
const cell_t env_sym    = INIT_SYM("interaction-environment");
const cell_t bind_sym   = INIT_SYM("secd-bind!");

const cell_t list_func  = INIT_FUNC(secdf_list);
const cell_t appnd_func = INIT_FUNC(secdf_append);
const cell_t copy_func  = INIT_FUNC(secdf_copy);
const cell_t nullp_func = INIT_FUNC(secdf_null);
const cell_t nump_func  = INIT_FUNC(secdf_nump);
const cell_t symp_func  = INIT_FUNC(secdf_symp);
const cell_t eofp_func  = INIT_FUNC(secdf_eofp);
const cell_t debug_func = INIT_FUNC(secdf_ctl);
const cell_t getenv_fun = INIT_FUNC(secdf_getenv);
const cell_t bind_func  = INIT_FUNC(secdf_bind);

const struct {
    const cell_t *sym;
    const cell_t *val;
} native_functions[] = {
    // native functions
    { &list_sym,    &list_func  },
    { &append_sym,  &appnd_func },
    { &nullp_sym,   &nullp_func },
    { &nump_sym,    &nump_func  },
    { &symp_sym,    &symp_func  },
    { &copy_sym,    &copy_func  },
    { &eofp_sym,    &eofp_func  },
    { &debug_sym,   &debug_func  },
    { &env_sym,     &getenv_fun },
    { &bind_sym,    &bind_func  },

    // symbols
    { &t_sym,       &t_sym      },
    { NULL,         NULL        } // must be last
};


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
