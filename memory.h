#ifndef __SECD_MEM_H__
#define __SECD_MEM_H__

#include "secd.h"

/*
 *   Allocation
 */

cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr);
cell_t *new_frame(secd_t *secd, cell_t *syms, cell_t *vals);
cell_t *new_number(secd_t *secd, int num);
cell_t *new_symbol(secd_t *secd, const char *sym);
cell_t *new_string(secd_t *secd, const char *str);
cell_t *new_string_of_size(secd_t *secd, size_t size);
cell_t *new_op(secd_t *secd, opindex_t opind);
cell_t *new_const_clone(secd_t *secd, const cell_t *from);
cell_t *new_clone(secd_t *secd, cell_t *from);
cell_t *new_error(secd_t *secd, const char *fmt, ...);
cell_t *new_array(secd_t *secd, size_t size);

cell_t *init_with_copy(
        secd_t *secd,
        cell_t *__restrict cell,
        const cell_t *__restrict with);
cell_t *drop_dependencies(secd_t *secd, cell_t *c);

cell_t *free_cell(secd_t *, cell_t *c);

cell_t *push_stack(secd_t *secd, cell_t *newc);
cell_t *pop_stack(secd_t *secd);

cell_t *set_control(secd_t *secd, cell_t *opcons);
cell_t *pop_control(secd_t *secd);

cell_t *push_dump(secd_t *secd, cell_t *cell);
cell_t *pop_dump(secd_t *secd);

/*
 * Reference-counting
 */

inline static cell_t *share_cell(secd_t __unused *secd, cell_t *c) {
    if (not_nil(c)) {
        ++c->nref;
        memtracef("share[%ld] %ld\n", cell_index(c), c->nref);
    } else {
        memtracef("share[NIL]\n");
    }
    return c;
}

inline static cell_t *drop_cell(secd_t *secd, cell_t *c) {
    if (is_nil(c)) {
        memtracef("drop [NIL]\n");
        return NULL;
    }
    if (c->nref <= 0) {
        assert(c->nref > 0, "drop_cell[%ld]: negative", cell_index(secd, c));
    }

    -- c->nref;
    memtracef("drop [%ld] %ld\n", cell_index(c), c->nref);
    if (c->nref) return c;
    return free_cell(secd, c);
}

static inline size_t arrmeta_size(secd_t *secd, cell_t *metacons) {
    asserti(cell_type(metacons) == CELL_ARRMETA, "arrmeta_size: not a meta");
    if (metacons == secd->arrlist) return 0;
    return get_car(metacons) - metacons - 1;
}

static inline cell_t *arr_meta(cell_t *arr) {
    if (cell_type(arr - 1) != CELL_ARRMETA) {
        errorf("arr_meta: not a meta");
        return SECD_NIL;
    }
    return arr - 1;
}

static inline size_t arr_size(secd_t *secd, cell_t *arr) {
    return arrmeta_size(secd, arr_meta(arr->as.arr.data));
}

/*
 *
 */

void init_mem(secd_t *secd, cell_t *heap, size_t size);

/*
 *    UTF-8
 */
typedef  unsigned long  unichar_t;

char *utf8cpy(char *to, unichar_t ucs);

size_t list_length(secd_t *secd, cell_t *lst);
cell_t *vector_from_list(secd_t *secd, cell_t *lst);

#endif // __SECD_MEM_H__
