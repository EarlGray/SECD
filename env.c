#include "secd.h"
#include "secd_io.h"
#include "env.h"
#include "memory.h"

#include <string.h>

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
            if (atom_type(secd, sym) != ATOM_SYM) {
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

    for (i = 0; not_nil(binding[i].sym); ++i) {
        cell_t *sym = new_const_clone(secd, binding[i].sym);
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
    secd->envcounter = 0;

    cell_t *frame = make_native_frame(secd, native_functions, ":secd");

    cell_t *frame_io = new_cons(secd, secd->input_port, secd->output_port);
    frame->as.frame.io = share_cell(secd, frame_io);

    cell_t *env = new_cons(secd, frame, SECD_NIL);

    secd->env = share_cell(secd, env);
    secd->global_env = secd->env;
}

static cell_t *lookup_fake_variables(secd_t *secd, const char *sym) {
    if (str_eq(sym, SECD_FAKEVAR_STDIN))
        return secd->input_port;
    if (str_eq(sym, SECD_FAKEVAR_STDOUT)) 
        return secd->output_port;
    if (str_eq(sym, SECD_FAKEVAR_STDDBG))
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
        if (str_eq(SECD_FAKEVAR_MODULE, symname(sym))) {
            cell_t *mod = list_head(vallist);
            if (atom_type(secd, mod) != ATOM_SYM) {
                errorf("Module name is not an atom");
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

static bool name_eq(const char *sym, const char *cur, const char *modname, size_t modlen, bool open) {
    if (open && str_eq(cur, sym))
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

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        if (is_nil(frame)) {
            env = list_next(secd, env);
            continue;
        }
        bool open;
        const char *mod = module_name_for_frame(secd, frame, &open);
        assert(mod, "lookup_env: no module name");
        size_t modlen = strlen(mod);

        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *curc = get_car(symlist);
            assert(atom_type(secd, curc) == ATOM_SYM, 
                   "lookup_env: variable at [%ld] is not a symbol\n", cell_index(secd, curc));

            if (name_eq(symbol, symname(curc), mod, modlen, open)) {
                if (symc != NULL) *symc = curc;
                return get_car(vallist);
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
            assert(atom_type(secd, cur_sym) == ATOM_SYM,
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
    while (not_nil(symlist)) {
        cell_t *sym = get_car(symlist);
        if (str_eq(symname(sym), "*stdin*")) {
            cell_t *val = get_car(vallist);
            assert(cell_type(val) == CELL_PORT, "*stdin* must bind a port");
            if (is_nil(frame_io))
                frame_io = new_cons(secd, val, SECD_NIL);
            else
                frame_io->as.cons.car = share_cell(secd, val);
        }
        if (str_eq(symname(sym), "*stdout*")) {
            cell_t *val = get_car(vallist);
            assert(cell_type(val) == CELL_PORT, "*stdout* must bind a port");
            if (is_nil(frame_io))
                frame_io = new_cons(secd, SECD_NIL, val);
            else
                frame_io->as.cons.cdr = share_cell(secd, val);
        }

        symlist = list_next(secd, symlist);
        vallist = list_next(secd, vallist);
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
    /* insert *module* variable into the new frame */
    cell_t *modsym = SECD_NIL;
    if (is_error(lookup_env(secd, SECD_FAKEVAR_MODULE, &modsym)))
        return new_error(secd, "there's no *module* variable");

    char envname[64];   /* determine name for the frame */
    snprintf(envname, 64, ":env%ld", secd->envcounter++);
    cell_t *modname = new_symbol(secd, envname);

    argnames = new_cons(secd, modsym, argnames);
    argvals = new_cons(secd, modname, argvals);

    /* setup the new frame */
    cell_t *frame = new_frame(secd, argnames, argvals);

    cell_t *new_io = new_frame_io(secd, frame, env);
    assert_cell(new_io, "setup_frame: failed to set new frame I/O\n");

    frame->as.frame.io = share_cell(secd, new_io);
    secd->input_port = get_car(new_io);
    secd->output_port = get_cdr(new_io);

    return frame;
}

