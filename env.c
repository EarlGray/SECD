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
    printf(";;Environment:\n");
    while (not_nil(env)) {
        printf(";;  Frame #%d:\n", i++);
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
            printf(";;    %s\t=>\t", symname(sym));
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
    secd->envcounter = 0;

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
    if (open && (symh == symhash(cursym)) && str_eq(cur, sym))
        return true;
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
            "lookup_env: environment is not a list\n");

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
                   "lookup_env: variable at [%ld] is not a symbol\n", cell_index(secd, curc));

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
    errorf("lookup_env: %s not found\n", symbol);
    return new_error(secd, "lookup failed for: '%s'", symbol);
}

cell_t *lookup_symenv(secd_t *secd, const char *symbol) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS,
            "lookup_symbol: environment is not a list\n");

    cell_t *res = lookup_fake_variables(secd, symbol);
    if (not_nil(res))
        return res;

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            assert(is_symbol(cur_sym),
                    "lookup_symbol: variable at [%ld] is not a symbol\n", cell_index(secd, cur_sym));

            if (str_eq(symbol, symname(cur_sym))) {
                return cur_sym;
            }
            symlist = list_next(secd, symlist);
        }

        env = list_next(secd, env);
    }
    return SECD_NIL;
}

static cell_t *new_frame_io(secd_t *secd, cell_t *frame, cell_t *prevenv) {
    /* check if there are *stdin* or *stdout* in new frame */
    cell_t *frame_io = SECD_NIL;
    cell_t *symlist = get_car(frame);
    cell_t *vallist = get_cdr(frame);

    if (is_symbol(symlist)) {
        frame->as.cons.car = share_cell(secd, new_cons(secd, symlist, SECD_NIL));
        frame->as.cons.cdr = share_cell(secd, new_cons(secd, vallist, SECD_NIL));
        drop_cell(secd, symlist); drop_cell(secd, vallist);
    } else
    while (not_nil(symlist)) {
        cell_t *sym = get_car(symlist);
        hash_t symh = symhash(sym);
        if ((symh == stdinhash)
            && str_eq(symname(sym), SECD_FAKEVAR_STDIN))
        {
            cell_t *val = get_car(vallist);
            assert(cell_type(val) == CELL_PORT, "*stdin* must bind a port");
            if (is_nil(frame_io))
                frame_io = new_cons(secd, val, SECD_NIL);
            else
                frame_io->as.cons.car = share_cell(secd, val);
        } else
        if ((symh == stdouthash)
            && str_eq(symname(sym), SECD_FAKEVAR_STDOUT))
        {
            cell_t *val = get_car(vallist);
            assert(cell_type(val) == CELL_PORT, "*stdout* must bind a port");
            if (is_nil(frame_io))
                frame_io = new_cons(secd, SECD_NIL, val);
            else
                frame_io->as.cons.cdr = share_cell(secd, val);
        }

        cell_t *nextsyms = list_next(secd, symlist);
        cell_t *nextvals = list_next(secd, vallist);

        /* dot-lists of arguments? */
        if (is_symbol(nextsyms)) {
            symlist->as.cons.cdr = share_cell(secd, new_cons(secd, nextsyms, SECD_NIL));
            vallist->as.cons.cdr = share_cell(secd, new_cons(secd, nextvals, SECD_NIL));
            drop_cell(secd, nextsyms); drop_cell(secd, nextvals);
            break;
        }

        symlist = nextsyms;
        vallist = nextvals;
    }
    cell_t *prev_io = get_car(prevenv)->as.frame.io;
    if (is_nil(frame_io))
        return prev_io;

    if (is_nil(get_car(frame_io)))
        frame_io->as.cons.car = share_cell(secd, get_car(prev_io));
    if (is_nil(get_cdr(frame_io)))
        frame_io->as.cons.cdr = share_cell(secd, get_cdr(prev_io));
    return frame_io;
}

cell_t *setup_frame(secd_t *secd, cell_t *argnames, cell_t *argvals, cell_t *env) {
    /* insert *module* variable into the new frame
    cell_t *modsym = SECD_NIL;
    if (is_error(lookup_env(secd, SECD_FAKEVAR_MODULE, &modsym)))
        return new_error(secd, "there's no *module* variable");

    char envname[64];   // determine name for the frame
    snprintf(envname, 64, ":env%ld", secd->envcounter++);
    cell_t *modname = new_symbol(secd, envname);

    argnames = new_cons(secd, modsym, argnames);
    argvals = new_cons(secd, modname, argvals); // */

    /* setup the new frame */
    cell_t *frame = new_frame(secd, argnames, argvals);

    cell_t *new_io = new_frame_io(secd, frame, env);
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
