#include "memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*
 *  Cell memory management
 */

bool is_control_compiled(cell_t *control);
cell_t *compile_control_path(secd_t *secd, cell_t *control);

cell_t *pop_free(secd_t *secd) {
    cell_t *cell = secd->free;
    assert(not_nil(secd, cell), "pop_free: no free memory");

    secd->free = list_next(secd, cell);
    memdebugf("NEW [%ld]\n", cell_index(secd, cell));
    -- secd->free_cells;

    cell->type = (intptr_t)secd;
    return cell;
}

void push_free(secd_t *secd, cell_t *c) {
    assertv(c, "push_free(NULL)");
    assertv(c->nref == 0, "push_free: [%ld]->nref is %ld\n", cell_index(secd, c), c->nref);
    c->type = (intptr_t)secd | CELL_CONS;
    c->as.cons.cdr = secd->free;
    secd->free = c;
    ++ secd->free_cells;
    memdebugf("FREE[%ld]\n", cell_index(secd, c));
}

cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr) {
    cell_t *cell = pop_free(secd);
    cell->type |= CELL_CONS;
    cell->as.cons.car = share_cell(secd, car);
    cell->as.cons.cdr = share_cell(secd, cdr);
    return cell;
}

cell_t *new_frame(secd_t *secd, cell_t *syms, cell_t *vals) {
    cell_t *cons = new_cons(secd, syms, vals);
    cons->type &= (INTPTR_MAX << SECD_ALIGN);
    cons->type |= CELL_FRAME;
    return cons;
}

cell_t *new_number(secd_t *secd, int num) {
    cell_t *cell = pop_free(secd);
    cell->type |= CELL_ATOM;
    cell->as.atom.type = ATOM_INT;
    cell->as.atom.as.num = num;
    return cell;
}

cell_t *new_symbol(secd_t *secd, const char *sym) {
    cell_t *cell = pop_free(secd);
    cell->type |= CELL_ATOM;
    cell->as.atom.type = ATOM_SYM;
    cell->as.atom.as.sym.size = strlen(sym);
    cell->as.atom.as.sym.data = strdup(sym);
    return cell;
}

cell_t *new_op(secd_t *secd, opindex_t opind) {
    cell_t *cell = pop_free(secd);
    cell->type |= CELL_ATOM;
    cell->as.atom.type = ATOM_OP;
    cell->as.atom.as.op = opind;
    return cell;
}

cell_t *new_clone(secd_t *secd, const cell_t *from) {
    if (!from) return NULL;
    cell_t *clone = pop_free(secd);
    memcpy(clone, from, sizeof(cell_t));
    clone->type = (intptr_t)secd | cell_type(from);
    clone->nref = 0;
    return clone;
}

cell_t *new_error(secd_t *secd, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
#define MAX_ERROR_SIZE  512
    char buf[MAX_ERROR_SIZE];
    vsnprintf(buf, MAX_ERROR_SIZE, fmt, va);
    va_end(va);

    cell_t *err = pop_free(secd);
    err->type |= CELL_ERROR;
    err->as.err.len = strlen(buf);
    err->as.err.msg = strdup(buf);
    return err;
}

void free_atom(cell_t *cell) {
    switch (cell->as.atom.type) {
      case ATOM_SYM:
        if (cell->as.atom.as.sym.size != DONT_FREE_THIS)
            free((char *)cell->as.atom.as.sym.data); break;
      default: return;
    }
}

cell_t *free_cell(secd_t *secd, cell_t *c) {
    enum cell_type t = cell_type(c);
    switch (t) {
      case CELL_ATOM:
        free_atom(c);
        break;
      case CELL_FRAME:
        drop_cell(secd, get_car(c));
        drop_cell(secd, get_cdr(c));
        break;
      case CELL_CONS:
        drop_cell(secd, get_car(c));
        drop_cell(secd, get_cdr(c));
        break;
      case CELL_ERROR:
        return c;
      default:
        return new_error(secd, "free_cell: unknown cell_type 0x%x", t);
    }
    push_free(secd, c);
    return NULL;
}

inline static cell_t *push(secd_t *secd, cell_t **to, cell_t *what) {
    cell_t *newtop = new_cons(secd, what, *to);
    drop_cell(secd, *to);
    return (*to = share_cell(secd, newtop));
}

inline static cell_t *pop(secd_t *secd, cell_t **from) {
    cell_t *top = *from;
    assert(not_nil(secd, top), "pop: stack is empty");
    assert(is_cons(top), "pop: not a cons");

    cell_t *val = share_cell(secd, get_car(top));
    *from = share_cell(secd, get_cdr(top));
    drop_cell(secd, top);
    return val;
}

cell_t *push_stack(secd_t *secd, cell_t *newc) {
    cell_t *top = push(secd, &secd->stack, newc);
    memdebugf("PUSH S[%ld (%ld, %ld)]\n", cell_index(secd, top),
                        cell_index(secd, get_car(top)), cell_index(secd, get_cdr(top)));
    return top;
}

cell_t *pop_stack(secd_t *secd) {
    cell_t *cell = pop(secd, &secd->stack);
    memdebugf("POP S[%ld]\n", cell_index(secd, cell));
    return cell; // don't forget to drop_call(result)
}

cell_t *set_control(secd_t *secd, cell_t *opcons) {
    assert(is_cons(opcons),
           "set_control: failed, not a cons at [%ld]\n", cell_index(secd, opcons));
    if (! is_control_compiled(opcons)) {
        opcons = compile_control_path(secd, opcons);
        assert(opcons, "set_control: failed to compile control path");
    }
    return (secd->control = share_cell(secd, opcons));
}

cell_t *pop_control(secd_t *secd) {
    return pop(secd, &secd->control);
}

cell_t *push_dump(secd_t *secd, cell_t *cell) {
    cell_t *top = push(secd, &secd->dump, cell);
    memdebugf("PUSH D[%ld] (%ld, %ld)\n", cell_index(secd, top),
            cell_index(secd, get_car(top), get_cdr(top)));
    return top;
}

cell_t *pop_dump(secd_t *secd) {
    cell_t *cell = pop(secd, &secd->dump);
    memdebugf("POP D[%ld]\n", cell_index(secd, cell));
    return cell;
}

void init_mem(secd_t *secd, size_t size) {
    /* mark up a list of free cells */
    int i;
    for (i = 0; i + 1 < (int)size; ++i) {
        cell_t *c = secd->data + i;
        c->type = (intptr_t)secd | CELL_CONS;
        c->as.cons.cdr = secd->data + i + 1;
    }
    cell_t * c = secd->data + size - 1;
    secd->nil = c;
    c->type = (intptr_t)secd | CELL_CONS;
    c->as.cons.cdr = NULL;
}
