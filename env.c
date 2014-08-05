#include "secd.h"
#include "secd_io.h"
#include "env.h"
#include "memory.h"

#include <string.h>

static hash_t stdinhash;
static hash_t stdouthash;
static hash_t modulehash;

/*
 *  Environment
 */

void print_env(secd_t *secd) {
    cell_t *env = secd->env;
    int i = 0;
    secd_printf(secd, ";;Environment:\n");
    while (not_nil(env)) {
        secd_printf(secd, ";;  Frame #%d:\n", i++);
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {
            cell_t *sym = get_car(symlist);
            cell_t *val = get_car(vallist);
            if (!is_symbol(sym)) {
                errorf("print_env: not a symbol at *%p in symlist\n", sym);
                dbg_printc(secd, sym);
            }
            secd_printf(secd, ";;    %s\t=>\t", symname(sym));
            dbg_print_cell(secd, val);

            symlist = list_next(secd, symlist);
            vallist = list_next(secd, vallist);
        }

        env = list_next(secd, env);
    }
}

cell_t *make_native_frame(secd_t *secd,
                          const native_binding_t *binding,
                          const char *framename)
{
    int i;
    cell_t *symlist = SECD_NIL;
    cell_t *vallist = SECD_NIL;

    for (i = 0; binding[i].name; ++i) {
        cell_t *sym = new_symbol(secd, binding[i].name);
        cell_t *val = new_const_clone(secd, binding[i].val);
        if (not_nil(val))
            sym->nref = val->nref = DONT_FREE_THIS;
        symlist = new_cons(secd, sym, symlist);
        vallist = new_cons(secd, val, vallist);
    }

    symlist = new_cons(secd, new_symbol(secd, SECD_FAKEVAR_MODULE), symlist);
    vallist = new_cons(secd, new_symbol(secd, framename), vallist);

    return new_frame(secd, symlist, vallist);
}

void init_env(secd_t *secd) {
    /* initialize global values */
    stdinhash = strhash(SECD_FAKEVAR_STDIN);
    stdouthash = strhash(SECD_FAKEVAR_STDOUT);
    modulehash = strhash(SECD_FAKEVAR_MODULE);

    /* initialize the first frame */
    cell_t *frame = make_native_frame(secd, native_functions, ":secd");

    cell_t *frame_io = new_cons(secd, secd->input_port, secd->output_port);
    frame->as.frame.io = share_cell(secd, frame_io);

    /* ready */
    cell_t *env = new_cons(secd, frame, SECD_NIL);

    secd->env = share_cell(secd, env);
    secd->global_env = secd->env;
}

static cell_t *lookup_fake_variables(secd_t *secd, const char *sym) {
    hash_t symh = strhash(sym);
    if ((symh == stdinhash) && str_eq(sym, SECD_FAKEVAR_STDIN))
        return secd->input_port;
    if ((symh == stdouthash) && str_eq(sym, SECD_FAKEVAR_STDOUT))
        return secd->output_port;
    if ((symh == modulehash) && str_eq(sym, SECD_FAKEVAR_STDDBG))
        return secd->debug_port;
    return SECD_NIL;
}

static const char *module_name_for_frame(secd_t *secd, cell_t *frame, bool *open) {
    if (cell_type(frame) != CELL_FRAME) {
        errorf("module_name_for_frame: not a frame\n");
        return NULL;
    }
    cell_t *symlist = get_car(frame);
    cell_t *vallist = get_cdr(frame);
    while (not_nil(symlist)) {
        cell_t *sym = list_head(symlist);
        if ((symhash(sym) == modulehash)
            && str_eq(SECD_FAKEVAR_MODULE, symname(sym)))
        {
            cell_t *mod = list_head(vallist);
            if (!is_symbol(mod)) {
                errorf("Module name is not a symbol");
                return NULL;
            }
            const char *symstr = symname(mod);
            if (symstr[0] == ':') {
                *open = true;
                return symstr + 1;
            } else {
                *open = false;
                return symstr;
            }
        }
        symlist = list_next(secd, symlist);
        vallist = list_next(secd, vallist);
    }
    return NULL;
}

static bool name_eq(const char *sym, hash_t symh, cell_t *cursym,
        const char *modname, size_t modlen, bool open)
{
    const char *cur = symname(cursym);
    if (open && (symh == symhash(cursym))) {
        if (symname(cursym) == sym)
           return true;
        if (str_eq(cur, sym))
            return true;
    }
    if (sym[modlen] != ':')
        return false;
    if (!strncmp(sym, modname, modlen - 1)) {
        if (str_eq(sym + modlen + 1, cur))
            return true;
    }
    return false;
}

cell_t *lookup_env(secd_t *secd, const char *symbol, cell_t **symc) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS,
            "lookup_env: environment is not a list");

    cell_t *res = lookup_fake_variables(secd, symbol);
    if (not_nil(res)) {
        return res;
    }
    hash_t symh = strhash(symbol);

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        if (is_nil(frame)) {
            env = list_next(secd, env);
            continue;
        }
        bool open = true;
        const char *mod = module_name_for_frame(secd, frame, &open);
        size_t modlen = (mod ? strlen(mod) : 0);

        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *curc = get_car(symlist);
            assert(is_symbol(curc),
                   "lookup_env: variable at [%ld] is not a symbol\n",
                   cell_index(secd, curc));

            if (mod) {
                if (name_eq(symbol, symh, curc, mod, modlen, open)) {
                    if (symc != NULL) *symc = curc;
                    return get_car(vallist);
                }
            } else {
                if (symh == symhash(curc) && str_eq(symbol, symname(curc))) {
                    if (symc != NULL) *symc = curc;
                    return get_car(vallist);
                }
            }

            symlist = list_next(secd, symlist);
            vallist = list_next(secd, vallist);
        }

        env = list_next(secd, env);
    }
    errorf(";; error in lookup_env(): %s not found\n", symbol);
    return new_error(secd, "lookup failed for: '%s'", symbol);
}

cell_t *lookup_symenv(secd_t *secd, const char *symbol) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS,
            ";; error in lookup_symbol(): environment is not a list");

    cell_t *res = lookup_fake_variables(secd, symbol);
    if (not_nil(res))
        return res;

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            assert(is_symbol(cur_sym),
                   ";; error in lookup_symbol(): variable at [%ld] is not a symbol",
                   cell_index(secd, cur_sym));

            if (str_eq(symbol, symname(cur_sym))) {
                return cur_sym;
            }
            symlist = list_next(secd, symlist);
        }

        env = list_next(secd, env);
    }
    return SECD_NIL;
}

static cell_t *
check_io_args(secd_t *secd, cell_t *sym, cell_t *val, cell_t **args_io) {
    /* check for overriden *stdin* or *stdout* */
    hash_t symh = symhash(sym);
    if ((symh == stdinhash)
        && str_eq(symname(sym), SECD_FAKEVAR_STDIN))
    {
        assert(cell_type(val) == CELL_PORT, "*stdin* must bind a port");
        if (is_nil(*args_io))
            *args_io = new_cons(secd, val, SECD_NIL);
        else
            (*args_io)->as.cons.car = share_cell(secd, val);
    } else
    if ((symh == stdouthash)
        && str_eq(symname(sym), SECD_FAKEVAR_STDOUT))
    {
        assert(cell_type(val) == CELL_PORT, "*stdout* must bind a port");
        if (is_nil(*args_io))
            *args_io = new_cons(secd, SECD_NIL, val);
        else
            (*args_io)->as.cons.cdr = share_cell(secd, val);
    }
    return SECD_NIL;
}

/* check arity;
 * possibly rewrite dot-lists into regular arguments;
 * look for overriden *stdin*|*stdout* */
static cell_t *
walk_through_arguments(secd_t *secd, cell_t *frame, cell_t **args_io) {
    cell_t *symlist = get_car(frame);
    cell_t *vallist = get_cdr(frame);

    size_t valcount = 0;

    if (is_symbol(symlist)) {
        /* ((lambda args <body>) arg1 arg2 ...)
         *  => (lambda (args='(arg1 arg2 ...)) <body>)*/
        frame->as.cons.car = share_cell(secd, new_cons(secd, symlist, SECD_NIL));
        frame->as.cons.cdr = share_cell(secd, new_cons(secd, vallist, SECD_NIL));
        drop_cell(secd, symlist); drop_cell(secd, vallist);
        return SECD_NIL;
    }

    while (not_nil(symlist)) {
        if (is_nil(vallist)) {
            errorf(";; arity mismatch: %zd argument(s) is not enough\n", valcount);
            return new_error(secd,
                    "arity mismatch: %zd argument(s) is not enough", valcount);
        }

        cell_t *sym = get_car(symlist);

        check_io_args(secd, sym, get_car(vallist), args_io);

        cell_t *nextsyms = list_next(secd, symlist);
        cell_t *nextvals = list_next(secd, vallist);

        /* dot-lists of arguments? */
        if (is_symbol(nextsyms)) {
            symlist->as.cons.cdr =
                share_cell(secd, new_cons(secd, nextsyms, SECD_NIL));
            vallist->as.cons.cdr =
                share_cell(secd, new_cons(secd, nextvals, SECD_NIL));
            drop_cell(secd, nextsyms); drop_cell(secd, nextvals);
            break;
        }

        ++valcount;

        symlist = nextsyms;
        vallist = nextvals;
    }

    return SECD_NIL;
}

/* use *args_io to override *stdin* | *stdout* if not NIL */
static cell_t *new_frame_io(secd_t *secd, cell_t *args_io, cell_t *prevenv) {
    cell_t *prev_io = get_car(prevenv)->as.frame.io;
    if (is_nil(args_io))
        return prev_io; /* share previous i/o */

    if (is_nil(get_car(args_io)))
        args_io->as.cons.car = share_cell(secd, get_car(prev_io));
    if (is_nil(get_cdr(args_io)))
        args_io->as.cons.cdr = share_cell(secd, get_cdr(prev_io));
    return args_io; /* set a new i/o */
}

cell_t *setup_frame(secd_t *secd, cell_t *argnames, cell_t *argvals, cell_t *env) {
    cell_t *args_io = SECD_NIL;

    /* setup the new frame */
    cell_t *frame = new_frame(secd, argnames, argvals);

    cell_t *ret = walk_through_arguments(secd, frame, &args_io);
    assert_cell(ret, "setup_frame: argument check failed");

    cell_t *new_io = new_frame_io(secd, args_io, env);
    assert_cell(new_io, "setup_frame: failed to set new frame I/O\n");

    frame->as.frame.io = share_cell(secd, new_io);
    secd->input_port = get_car(new_io);
    secd->output_port = get_cdr(new_io);

    return frame;
}

cell_t *secd_insert_in_frame(secd_t *secd, cell_t *frame, cell_t *sym, cell_t *val) {
    cell_t *old_syms = get_car(frame);
    cell_t *old_vals = get_cdr(frame);

    // an interesting side effect: since there's no check for
    // re-binding an existing symbol, we can create multiple
    // copies of it on the frame, the last added is found
    // during value lookup, but the old ones are persistent
    frame->as.cons.car = share_cell(secd, new_cons(secd, sym, old_syms));
    frame->as.cons.cdr = share_cell(secd, new_cons(secd, val, old_vals));

    drop_cell(secd, old_syms); drop_cell(secd, old_vals);
    return frame;
}
