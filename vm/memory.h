#ifndef __SECD_MEM_H__
#define __SECD_MEM_H__

#include "secd.h"
#include "secd_io.h"

/*
 *   Allocation
 */

cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr);
cell_t *new_frame(secd_t *secd, cell_t *syms, cell_t *vals);
cell_t *new_number(secd_t *secd, int num);
cell_t *new_char(secd_t *secd, int chr);
cell_t *new_symbol(secd_t *secd, const char *sym);

cell_t *new_array(secd_t *secd, size_t size);
cell_t *new_array_for(secd_t *secd, cell_t *mem);

cell_t *new_string(secd_t *secd, const char *str);
cell_t *new_string_of_size(secd_t *secd, size_t size);
cell_t *new_strref(secd_t *secd, cell_t *mem, size_t size);

cell_t *new_bytevector_of_size(secd_t *secd, size_t size);

cell_t *new_ref(secd_t *secd, cell_t *to);
cell_t *new_op(secd_t *secd, opindex_t opind);

cell_t *new_port(secd_t *secd, int pty);

cell_t *new_const_clone(secd_t *secd, const cell_t *from);
cell_t *new_clone(secd_t *secd, cell_t *from);

cell_t *new_error(secd_t *secd, cell_t *info, const char *fmt, ...);
cell_t *new_continuation(secd_t *secd, cell_t *s, cell_t *e, cell_t *c);
cell_t *new_current_continuation(secd_t *secd);

cell_t *copy_value(
        secd_t *secd,
        cell_t *__restrict cell,
        const cell_t *__restrict with);
cell_t *drop_value(secd_t *secd, cell_t *c);

cell_t *free_cell(secd_t *, cell_t *c);

cell_t *push_stack(secd_t *secd, cell_t *newc);
cell_t *pop_stack(secd_t *secd);

cell_t *set_control(secd_t *secd, cell_t **opcons);
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

inline static int drop_cell(secd_t *secd, cell_t *c) {
    if (is_nil(c)) {
        memtracef("drop [NIL]\n");
        return 1;
    }
    if (c->nref <= 0) {
        errorf(";; %lu | error in drop_cell[%ld]: negative nref\n",
                secd->tick, cell_index(secd, c));
        return -1;
    }

    -- c->nref;
    memtracef("drop [%ld] %ld\n", cell_index(c), c->nref);
    if (c->nref) return 0;
    free_cell(secd, c);
    return 0;
}

inline static cell_t *assign_cell(secd_t *secd, cell_t **cell, cell_t *what) {
    cell_t *oldval = *cell;
    *cell = share_cell(secd, what);
    drop_cell(secd, oldval);
    return *cell;
}

cell_t *secd_referers_for(secd_t *secd, cell_t *cell);
void secd_owned_cell_for(secd_t *secd, cell_t *cell, cell_t **ref1, cell_t **ref2, cell_t **ref3);

/*
 *    Array routines
 */
static inline size_t arrmeta_size(secd_t *secd, const cell_t *metacons) {
    asserti(cell_type(metacons) == CELL_ARRMETA, "arrmeta_size: not a meta");
    if (metacons == secd->arrlist) return 0;
    return metacons->as.mcons.prev - metacons - 1;
}

static inline cell_t *arr_meta(cell_t *mem) {
    if (cell_type(mem - 1) != CELL_ARRMETA) {
        return SECD_NIL;
    }
    return mem - 1;
}

static inline cell_t *meta_mem(cell_t *meta) {
    if (cell_type(meta) != CELL_ARRMETA) {
       return SECD_NIL;
    }
    return meta + 1;
}

static inline cell_t *arr_mem(const cell_t *arr) {
    if (cell_type(arr) != CELL_ARRAY) {
        return SECD_NIL;
    }
    return arr->as.arr.data;
}

static inline size_t mem_size(const cell_t *str) {
    switch (cell_type(str)) {
      case CELL_STR: case CELL_BYTES: break;
      default: return -1;
    }
    return str->as.str.size;
}

static inline const cell_t *
arr_val(const cell_t *arr, size_t index) {
    if (cell_type(arr) != CELL_ARRAY) {
        return SECD_NIL;
    }
    return arr->as.arr.data + index;
}

static inline cell_t *
arr_ref(cell_t *arr, size_t index) {
    return arr_mem(arr) + index;
}

static inline cell_t *
arr_get(secd_t *secd, cell_t *arr, size_t index) {
    return new_clone(secd, arr_ref(arr, index));
}

static inline cell_t *
arr_set(secd_t *secd, cell_t *arr, size_t index, const cell_t *val) {
    cell_t *ref = arr_ref(arr, index);
    drop_value(secd, ref);
    copy_value(secd, ref, val);
    return arr;
}

static inline size_t arr_size(secd_t *secd, const cell_t *arr) {
    return arrmeta_size(secd, arr_val(arr, -1));
}

cell_t *fill_array(secd_t *secd, cell_t *arr, cell_t *with);

/*
 *    Global machine operations
 */

void secd_mark_and_sweep_gc(secd_t *secd);

void secd_init_mem(secd_t *secd, cell_t *heap, size_t size);

/*
 *    Hashtables
 */

bool secdht_is(secd_t *secd, cell_t *obj);

cell_t *secdht_new(secd_t *secd, int initcap, cell_t *eqfun, cell_t *hashfun);

cell_t *secdht_insert(secd_t *secd, cell_t *ht, cell_t *key, cell_t *val);

bool secdht_lookup(secd_t *secd, cell_t *ht, cell_t *key, cell_t **val);

cell_t *secdht_fold(secd_t *secd, cell_t *ht, cell_t *val, cell_t *iter);

/*
 *    UTF-8
 */
typedef  unsigned int  unichar_t;

char *utf8cpy(char *to, unichar_t ucs);
unichar_t utf8get(const char *u8, const char **next);

size_t list_length(secd_t *secd, cell_t *lst);
cell_t *list_to_vector(secd_t *secd, cell_t *lst);
cell_t *vector_to_list(secd_t *secd, cell_t *vct, int start, int end);

#endif // __SECD_MEM_H__
