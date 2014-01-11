#include "secd.h"
#include "memory.h"
#include "env.h"

#include <string.h>

/*
 * Some native functions
 */

static inline cell_t *to_bool(secd_t *secd, bool cond) {
    return ((cond)? lookup_env(secd, "#t") : SECD_NIL);
}

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
    return to_bool(secd, atom_type(secd, list_head(args)) == ATOM_INT);
}

cell_t *secdf_symp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_symp\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, atom_type(secd, list_head(args)) == ATOM_SYM);
}

static cell_t *list_copy(secd_t *secd, cell_t *list, cell_t **out_tail) {
    if (is_nil(list))
        return SECD_NIL;

    cell_t *new_head, *new_tail;
    new_head = new_tail = new_cons(secd, list_head(list), SECD_NIL);

    while (not_nil(list = list_next(secd, list))) {
        cell_t *new_cell = new_cons(secd, get_car(list), SECD_NIL);
        new_tail->as.cons.cdr = share_cell(secd, new_cell);
        new_tail = list_next(secd, new_tail);
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
    if (is_nil(args)) return args;
    assert(is_cons(args), "secdf_append: expected arguments");

    cell_t *xs = list_head(args);
    assert(is_cons(list_next(secd, args)), "secdf_append: expected two arguments");

    cell_t *argtail = list_next(secd, args);
    if (is_nil(argtail)) return xs;

    cell_t *ys = list_head(argtail);
    if (not_nil(list_next(secd, argtail))) {
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
        if (is_nil(list_next(secd, sum_tail)))
            break;
        sum_tail = list_next(secd, sum_tail);
    }

    if (sum_tail) {
        ctrldebugf("secdf_append: destructive append\n");
        sum_tail->as.cons.cdr = share_cell(secd, ys);
        sum = xs;
    } else {
        ctrldebugf("secdf_append: copying append\n");
        cell_t *sum_tail;
        sum = list_copy(secd, xs, &sum_tail);
        sum_tail->as.cons.cdr = share_cell(secd, ys);
    }

    return sum;
}

cell_t *secdf_eofp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_eofp\n");
    cell_t *arg1 = list_head(args);
    if (atom_type(secd, arg1) != ATOM_SYM)
        return SECD_NIL;
    return to_bool(secd, str_eq(symname(arg1), EOF_OBJ));
}

cell_t *secdf_ctl(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_ctl\n");
    if (is_nil(args))
        goto help;

    cell_t *arg1 = list_head(args);
    if (atom_type(secd, arg1) == ATOM_SYM) {
        if (str_eq(symname(arg1), "free")) {
            printf(";; SECDCTL: \n");
            printf(";;  size = %ld\n", secd->end - secd->begin);
            printf(";;  fixedptr = %ld\n", secd->fixedptr - secd->begin);
            printf(";;  arrayptr = %ld\n", secd->arrayptr - secd->begin);
            printf(";;  Fixed cells: %ld free, %ld dump\n", 
                    secd->free_cells, secd->used_dump);
        } else if (str_eq(symname(arg1), "env")) {
            print_env(secd);
        } else if (str_eq(symname(arg1), "tick")) {
            printf("SECDCTL: tick = %lu\n", secd->tick);
            return new_number(secd, secd->tick);
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
    assert(atom_type(secd, sym) == ATOM_SYM, "secdf_bind: a symbol must be bound");

    args = list_next(secd, args);
    assert(not_nil(args), "secdf_bind: No value for binding");
    cell_t *val = list_head(args);

    cell_t *env;
    // is there the third argument?
    if (not_nil(list_next(secd, args))) {
        args = list_next(secd, args);
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
    frame->as.cons.car = share_cell(secd, new_cons(secd, sym, old_syms));
    frame->as.cons.cdr = share_cell(secd, new_cons(secd, val, old_vals));

    drop_cell(secd, old_syms); drop_cell(secd, old_vals);
    return sym;
}

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

const cell_t t_sym      = INIT_SYM("#t");
const cell_t nil_sym    = INIT_SYM("NIL");

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

cell_t * make_frame_of_natives(secd_t *secd) {
    int i;
    cell_t *symlist = SECD_NIL;
    cell_t *vallist = SECD_NIL;

    for (i = 0; native_functions[i].sym; ++i) {
        cell_t *sym = new_clone(secd, native_functions[i].sym);
        cell_t *val = new_clone(secd, native_functions[i].val);
        sym->nref = val->nref = DONT_FREE_THIS;
        cell_t *closure = new_cons(secd, val, SECD_NIL);
        symlist = new_cons(secd, sym, symlist);
        vallist = new_cons(secd, closure, vallist);
    }

    cell_t *sym = new_clone(secd, &nil_sym);
    cell_t *val = SECD_NIL;
    symlist = new_cons(secd, sym, symlist);
    vallist = new_cons(secd, val, vallist);

    return new_frame(secd, symlist, vallist);
}
