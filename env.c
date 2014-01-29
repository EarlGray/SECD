#include "secd.h"
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

void init_env(secd_t *secd) {
    cell_t *frame = make_frame_of_natives(secd);
    cell_t *env = new_cons(secd, frame, SECD_NIL);

    secd->env = share_cell(secd, env);
    secd->global_env = secd->env;
}

cell_t *lookup_env(secd_t *secd, const char *symbol) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS, 
            "lookup_env: environment is not a list\n");
    size_t symlen = strlen(symbol);

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        if (is_nil(frame)) {
            env = list_next(secd, env);
            continue;
        }
        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            if (atom_type(secd, cur_sym) != ATOM_SYM) {
                errorf("lookup_env: variable at [%ld] is not a symbol\n",
                        cell_index(secd, cur_sym));
                symlist = list_next(secd, symlist); 
                vallist = list_next(secd, vallist);
                continue;
            }
            if (cur_sym->as.atom.as.sym.size == symlen)
                if (str_eq(symbol, symname(cur_sym)))
                    return get_car(vallist);

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

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            assert(atom_type(secd, cur_sym) != ATOM_SYM,
                    "lookup_symbol: variable at [%ld] is not a symbol\n", cell_index(secd, cur_sym));

            if (str_eq(symbol, symname(cur_sym))) {
                return cur_sym;
            }
            symlist = list_next(secd, symlist);
        }

        env = list_next(secd, env);
    }
    return NULL;
}


