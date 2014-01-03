#ifndef __SECD_MEM_H__
#define __SECD_MEM_H__

#include "conf.h"
#include "secd.h"

/*
 *   Allocation
 */

cell_t *pop_free(secd_t *secd);
void push_free(cell_t *c);

cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr);
cell_t *new_frame(secd_t *secd, cell_t *syms, cell_t *vals);
cell_t *new_number(secd_t *secd, int num);
cell_t *new_symbol(secd_t *secd, const char *sym);
cell_t *new_clone(secd_t *secd, const cell_t *from);
cell_t *new_error(secd_t *secd, const char *fmt, ...);

cell_t *free_cell(cell_t *c);

/*
 * Reference-counting
 */

inline static cell_t *share_cell(cell_t *c) {
    if (not_nil(c)) {
        ++c->nref;
        memtracef("share[%ld] %ld\n", cell_index(c), c->nref);
    } else {
        memdebugf("share[NIL]\n");
    }
    return c;
}

inline static cell_t *drop_cell(cell_t *c) {
    if (is_nil(c)) {
        memdebugf("drop [NIL]\n");
        return NULL;
    }
    if (c->nref <= 0) {
        assert(c->nref > 0, "drop_cell[%ld]: negative", cell_index(c));
    }

    -- c->nref;
    memtracef("drop [%ld] %ld\n", cell_index(c), c->nref);
    if (c->nref) return c;
    return free_cell(c);
}

#endif // __SECD_MEM_H__
