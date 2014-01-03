#include "secd.h"
#include "env.h"

#include "memory.h"
#include "native.h"

#include <string.h>

/*
 *  Environment
 */

void print_env(secd_t *secd) {
    cell_t *env = secd->env;
    int i = 0;
    printf("Environment:\n");
    while (not_nil(env)) {
        printf(" Frame #%d:\n", i++);
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {
            cell_t *sym = get_car(symlist);
            cell_t *val = get_car(vallist);
            if (atom_type(sym) != ATOM_SYM)
                fprintf(stderr, "print_env: not a symbol at *%p in vallist\n", sym);
            printf("  %s\t=>\t", symname(sym));
            print_cell(val);

            symlist = list_next(symlist);
            vallist = list_next(vallist);
        }

        env = list_next(env);
    }
}

void init_env(secd_t *secd) {
    cell_t *frame = make_frame_of_natives(secd);
    cell_t *env = new_cons(secd, frame, secd->nil);

    secd->env = share_cell(env);
    secd->global_env = secd->env;
}

cell_t *lookup_env(secd_t *secd, const char *symbol) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS, "lookup_env: environment is not a list\n");

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        if (is_nil(frame)) {
            //printf("lookup_env: warning: skipping OMEGA-frame...\n");
            env = list_next(env);
            continue;
        }
        cell_t *symlist = get_car(frame);
        cell_t *vallist = get_cdr(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            if (atom_type(cur_sym) != ATOM_SYM) {
                errorf("lookup_env: variable at [%ld] is not a symbol\n",
                        cell_index(cur_sym));
                symlist = list_next(symlist); vallist = list_next(vallist);
                continue;
            }

            if (str_eq(symbol, symname(cur_sym))) {
                return get_car(vallist);
            }
            symlist = list_next(symlist);
            vallist = list_next(vallist);
        }

        env = list_next(env);
    }
    printf("lookup_env: %s not found\n", symbol);
    return NULL;
}

cell_t *lookup_symenv(secd_t *secd, const char *symbol) {
    cell_t *env = secd->env;
    assert(cell_type(env) == CELL_CONS, "lookup_symbol: environment is not a list\n");

    while (not_nil(env)) {       // walk through frames
        cell_t *frame = get_car(env);
        cell_t *symlist = get_car(frame);

        while (not_nil(symlist)) {   // walk through symbols
            cell_t *cur_sym = get_car(symlist);
            assert(atom_type(cur_sym) != ATOM_SYM,
                    "lookup_symbol: variable at [%ld] is not a symbol\n", cell_index(cur_sym));

            if (str_eq(symbol, symname(cur_sym))) {
                return cur_sym;
            }
            symlist = list_next(symlist);
        }

        env = list_next(env);
    }
    return NULL;
}


