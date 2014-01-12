#include "memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*
 *      A short description of SECD memory layout 
 *                                          last updated: 12 Jan 2014
 *
 *  TODO
 */

/* extern declarations */
bool is_control_compiled(cell_t *control);
cell_t *compile_control_path(secd_t *secd, cell_t *control);

/* internal declarations */
void free_array(secd_t *secd, cell_t *this);

/*
 *  Cell memory management
 */

cell_t *init_with_copy(secd_t *secd, cell_t *cell, cell_t *with) {
    memcpy(cell, with, sizeof(cell_t));

    cell->type = (intptr_t)secd | cell_type(with);
    cell->nref = 0;
    switch (cell_type(with)) {
      case CELL_CONS: case CELL_FRAME:
        share_cell(secd, with->as.cons.car);
        share_cell(secd, with->as.cons.cdr);
        break;
      case CELL_REF: 
        share_cell(secd, with->as.ref); 
        break;
      case CELL_ARRAY: 
        share_cell(secd, arr_meta(with->as.arr)); 
        break;
      case CELL_ERROR: 
      case CELL_ATOM:
      case CELL_UNDEF:
        break;
      case CELL_ARRMETA: case CELL_FREE: 
        return new_error(secd, "trying to initialize with CELL_ARRMETA");
    }
    return cell;
}

void free_atom(cell_t *cell) {
    switch (cell->as.atom.type) {
      case ATOM_SYM:
        if (cell->as.atom.as.sym.size != DONT_FREE_THIS)
            free((char *)cell->as.atom.as.sym.data); break;
      default: return;
    }
}

static cell_t *drop_dependencies(secd_t *secd, cell_t *c) {
    enum cell_type t = cell_type(c);
    switch (t) {
      case CELL_ATOM:
        free_atom(c);
        break;
      case CELL_FRAME:
      case CELL_CONS:
        drop_cell(secd, get_car(c));
        drop_cell(secd, get_cdr(c));
        break;
      case CELL_ARRAY: {
        cell_t *meta = arr_meta(c->as.arr);
        -- meta->nref;
        if (0 == meta->nref) {
            free_array(secd, c->as.arr);
        }
        } break;
      case CELL_REF:
        drop_cell(secd, c->as.ref);
        break;
      case CELL_ERROR:
        return c;
      default:
        return new_error(secd, "free_cell: unknown cell_type 0x%x", t);
    }
    return c;
}


cell_t *pop_free(secd_t *secd) {
    cell_t *cell;
    if (not_nil(secd->free)) {
        /* take a cell from the list */
        cell = secd->free;
        secd->free = get_cdr(secd->free);
        if (secd->free)
            secd->free->as.cons.car = SECD_NIL;
        memdebugf("NEW [%ld]\n", cell_index(secd, cell));
        -- secd->free_cells;
    } else {
        /* move fixedptr */
        if (secd->fixedptr >= secd->arrayptr)
            return &secd_out_of_memory;

        cell = secd->fixedptr;
        ++ secd->fixedptr;
        memdebugf("NEW [%ld] ++\n", cell_index(secd, cell));
    }
    cell->type = (intptr_t)secd;
    cell->nref = 0;
    return cell;
}

void push_free(secd_t *secd, cell_t *c) {
    assertv(c, "push_free(NULL)");
    assertv(c->nref == 0,
            "push_free: [%ld]->nref is %ld\n", cell_index(secd, c), c->nref);

    if (c + 1 < secd->fixedptr) {
        /* just add the cell to the list secd->free */
        if (not_nil(secd->free))
            secd->free->as.cons.car = c;
        c->type = CELL_FREE;
        c->as.cons.car = SECD_NIL;
        c->as.cons.cdr = secd->free;
        secd->free = c;

        ++secd->free_cells;
        memdebugf("FREE[%ld], %ld free\n", 
                cell_index(secd, c), secd->free_cells);
    } else {
        /* it is a cell adjacent to the free space */
        c->type = CELL_FREE;
        while (c->type == CELL_FREE) {
            if (c != secd->free) {
                cell_t *prev = c->as.cons.car;
                cell_t *next = c->as.cons.cdr;
                if (not_nil(prev)) {
                    prev->as.cons.cdr = next;
                }
                if (not_nil(next)) {
                    next->as.cons.car = prev;
                }
            } else {
                cell_t *prev = c->as.cons.car;
                if (not_nil(prev))
                    prev->as.cons.cdr = SECD_NIL;
                secd->free = prev;
            }
            memdebugf("FREE[%ld] --\n", cell_index(secd, c));
            --c;
            --secd->free_cells;
        }

        secd->fixedptr = c + 1;
        memdebugf("FREE[], %ld free\n", secd->free_cells);
    }
}

static inline cell_t*
init_cons(secd_t *secd, cell_t *cell, cell_t *car, cell_t *cdr) {
    cell->type = (intptr_t)secd | CELL_CONS;
    cell->as.cons.car = share_cell(secd, car);
    cell->as.cons.cdr = share_cell(secd, cdr);
    return cell;
}

static cell_t *init_meta(secd_t *secd, cell_t *cell, cell_t *prev, cell_t *next) {
    init_cons(secd, cell, prev, next);
    cell->type = (intptr_t)secd | CELL_ARRMETA;
    return cell;
}

/*
 *      Array memory management
 */

/* checks if the array described by the metadata cons is free */
static inline bool is_array_free(secd_t *secd, cell_t *metacons) {
    if (metacons == secd->arrlist) return false;
    return metacons->nref == 0;
}
static inline void mark_free(cell_t *metacons) { metacons->nref = 0; }
static inline void mark_used(cell_t *metacons) { metacons->nref = 1; }

cell_t *alloc_array(secd_t *secd, size_t size) {
    /* look through the list of arrays */
    cell_t *cur = secd->arrlist;
    while (not_nil(get_cdr(cur))) {
        if (is_array_free(secd, cur)) {
            if (arrmeta_size(secd, cur) >= size) {
                /* allocate this gap */
                return ;
            }
        }
        cur = get_cdr(cur);
    }

    /* no chunks of sufficient size found, move secd->arrayptr */
    if (secd->arrayptr - secd->fixedptr <= size)
        return &secd_out_of_memory;

    /* create new metadata cons at arrayptr - size - 1 */
    cell_t *oldmeta = secd->arrayptr;

    cell_t *meta = oldmeta - size - 1;
    init_meta(secd, meta, oldmeta, SECD_NIL);
    mark_used(meta);

    oldmeta->as.cons.cdr = meta;

    secd->arrayptr = meta;
    return meta + 1;
}

void free_array(secd_t *secd, cell_t *this) {
    assertv(this < secd->arrlist, "free_array: tried to free arrlist");
    assertv(secd->arrayptr < this, "free_array: not an array");

    cell_t *meta = arr_meta(this);
    cell_t *prev = get_car(meta);
    size_t size = arrmeta_size(secd, meta);
    int i;

    /* free the items */
    for (i = 0; i < size; ++i) {
        if (this[i].type != CELL_UNDEF) /* don't free uninitialized */
            drop_dependencies(secd, this + i);
    }

    if (meta != secd->arrayptr) {
        if (is_array_free(secd, prev)) {
            /* merge with the previous array */
            cell_t *pprev = get_car(prev);
            pprev->as.cons.cdr = this;
            this->as.cons.car = pprev;
        }

        cell_t *next = get_cdr(this);
        if (is_array_free(secd, next)) {
            /* merge with the next array */
            cell_t *newprev = get_car(this);
            next->as.cons.car = newprev;
            newprev->as.cons.cdr = next;
        }
        mark_free(this);
    } else {
        /* move arrayptr into the array area */
        prev->as.cons.cdr = SECD_NIL;
        secd->arrayptr = prev;

        if (is_array_free(secd, prev)) {
            /* at most one array after 'arr' may be free */
            cell_t *pprev = get_car(prev);
            pprev->as.cons.cdr = SECD_NIL;
            secd->arrayptr = pprev;
        }
    }
}

void print_array_layout(secd_t *secd) {
    errorf(";; Array heap layout:\n");
    errorf(";;  arrayptr = %ld\n", cell_index(secd, secd->arrayptr));
    errorf(";;  arrlist  = %ld\n", cell_index(secd, secd->arrlist));
    errorf(";; Array list is:\n");
    cell_t *cur = secd->arrlist;
    while (get_cdr(cur)) {
        cur = get_cdr(cur);
        errorf(";;  %ld\t%ld (size=%ld,\t%s)\n", 
                cell_index(secd, cur), cell_index(secd, cur->as.cons.car),
                arrmeta_size(secd, cur), (is_array_free(secd, cur)? "free" : "used"));
    }
}

/*
 *      "Constructors"
 */
cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr) {
    return init_cons(secd, pop_free(secd), car, cdr);
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

cell_t *new_const_clone(secd_t *secd, const cell_t *from) {
    if (!from) return NULL;

    cell_t *clone = pop_free(secd);
    memcpy(clone, from, sizeof(cell_t));
    clone->type = (intptr_t)secd | cell_type(from);
    clone->nref = 0;
    return clone;
}

static cell_t *init_error(cell_t *cell, const char *buf) {
    cell->type |= CELL_ERROR;
    cell->as.err.len = strlen(buf);
    cell->as.err.msg = strdup(buf);
    return cell;
}

cell_t *new_errorv(secd_t *secd, const char *fmt, va_list va) {
#define MAX_ERROR_SIZE  512
    char buf[MAX_ERROR_SIZE];
    vsnprintf(buf, MAX_ERROR_SIZE, fmt, va);
    return init_error(pop_free(secd), buf);
}

cell_t *new_error(secd_t *secd, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    cell_t *cell = new_errorv(secd, fmt, va);
    va_end(va);
    return cell;
}

cell_t *new_error_with(
        secd_t *secd, __unused cell_t *preverr, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    cell_t *err = new_errorv(secd, fmt, va);
    va_end(va);
    return err;
}

cell_t *new_array(secd_t *secd, size_t size) {
    /* try to allocate memory */
    cell_t *mem = alloc_array(secd, size);
    assert_cell(mem, "new_array: memory allocation failed");

    cell_t *arr = pop_free(secd);
    arr->type |= CELL_ARRAY;
    arr->as.arr = mem;
    mem->nref = 1;
    return arr;
}


cell_t *free_cell(secd_t *secd, cell_t *c) {
    push_free(secd, drop_dependencies(secd, c));
    return SECD_NIL;
}

inline static cell_t *push(secd_t *secd, cell_t **to, cell_t *what) {
    cell_t *newtop = new_cons(secd, what, *to);
    drop_cell(secd, *to);
    return (*to = share_cell(secd, newtop));
}

inline static cell_t *pop(secd_t *secd, cell_t **from) {
    cell_t *top = *from;
    assert(not_nil(top), "pop: stack is empty");
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
        assert_cell(opcons, "set_control: failed to compile control path");
    }
    return (secd->control = share_cell(secd, opcons));
}

cell_t *pop_control(secd_t *secd) {
    return pop(secd, &secd->control);
}

cell_t *push_dump(secd_t *secd, cell_t *cell) {
    cell_t *top = push(secd, &secd->dump, cell);
    memdebugf("PUSH D[%ld] (%ld, %ld)\n", cell_index(secd, top),
            cell_index(secd, get_car(top)), 
            cell_index(secd, get_cdr(top)));
    ++secd->used_dump;
    return top;
}

cell_t *pop_dump(secd_t *secd) {
    cell_t *cell = pop(secd, &secd->dump);
    memdebugf("POP D[%ld]\n", cell_index(secd, cell));
    --secd->used_dump;
    return cell;
}

void init_mem(secd_t *secd, cell_t *heap, size_t size) {
    secd->begin = heap;
    secd->end = heap + size;

    secd->fixedptr = secd->begin;
    secd->arrayptr = secd->end - 1;

    secd->arrlist = secd->arrayptr;
    init_meta(secd, secd->arrlist, SECD_NIL, SECD_NIL);
    secd->arrlist->nref = 0;

    secd->used_stack = 0;
    secd->used_dump = 0;
    secd->used_control = 0;
    secd->free_cells = 0;
}
