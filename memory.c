#include "memory.h"
#include "secd_io.h"
#include "secdops.h"

#include <string.h>
#include <stdarg.h>

/*
 *      A short description of SECD memory layout
 *                                          last updated: 12 Jan 2014
 *
 *  TODO
 */

/* internal declarations */
void free_array(secd_t *secd, cell_t *this);
int push_free(secd_t *secd, cell_t *c);

inline static cell_t *share_array(secd_t *secd, cell_t *mem) {
    share_cell(secd, arr_meta(mem));
    return mem;
}

inline static int drop_array(secd_t *secd, cell_t *mem) {
    cell_t *meta = arr_meta(mem);
    -- meta->nref;
    if (0 == meta->nref) {
        if (meta->as.mcons.cells) {
            size_t size = arrmeta_size(secd, meta);
            size_t i;

            /* free the items */
            for (i = 0; i < size; ++i) {
                /* don't free uninitialized */
                cell_t *ith = meta_mem(meta) + i;
                if (cell_type(ith) != CELL_UNDEF)
                    drop_value(secd, ith);
            }
        }
        free_array(secd, mem);
        return 1;
    }
    return 0;
}

/*
 *  Utilities
 */
inline static size_t bytes_to_cell(size_t bytes) {
    size_t ncell = bytes / sizeof(cell_t);
    if (bytes % sizeof(cell_t))
        ++ncell;
    return ncell;
}

/* http://en.wikipedia.org/wiki/Jenkins_hash_function */
static hash_t jenkins_hash(const char *key, size_t len) {
    uint32_t hash, i;
    for(hash = i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

hash_t strhash(const char *strz) {
    uint32_t hash = 0;
    while (*strz) {
        hash += *strz;
        hash += (hash << 10);
        hash ^= (hash >> 6);
        ++strz;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

hash_t memhash(const char *key, size_t len) {
    return jenkins_hash(key, len);
}

/*
 *  Cell memory management
 */

/* Deallocation */
cell_t *drop_value(secd_t *secd, cell_t *c) {
    enum cell_type t = cell_type(c);
    switch (t) {
      case CELL_FRAME:
        drop_cell(secd, c->as.frame.io);
        // fall through
      case CELL_CONS:
        if (not_nil(c)) {
            drop_cell(secd, get_car(c));
            drop_cell(secd, get_cdr(c));
        }
        break;
      case CELL_STR: case CELL_BYTES:
        drop_array(secd, (cell_t*)strmem(c));
        break;
      case CELL_ARRAY:
        drop_array(secd, arr_mem(c));
        break;
      case CELL_REF:
        drop_cell(secd, c->as.ref);
        break;
      case CELL_PORT:
        if (!is_closed(c))
            secd_pclose(secd, c);
        break;
      case CELL_KONT:
        drop_cell(secd, c->as.kont.stack);
        drop_cell(secd, c->as.kont.env);
        drop_cell(secd, c->as.kont.ctrl);
        break;
      case CELL_SYM:
        drop_cell(secd, c->as.sym.bvect);
        break;
      case CELL_INT: case CELL_CHAR: case CELL_FUNC:
      case CELL_OP: case CELL_ERROR: case CELL_UNDEF:
        return c;
      default:
        errorf("drop_value: unknown cell type %d\n", t);
        return SECD_NIL;
    }
    return c;
}

cell_t *free_cell(secd_t *secd, cell_t *c) {
    drop_value(secd, c);
    push_free(secd, c);
    return SECD_NIL;
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
        -- secd->stat.free_cells;
    } else {
        assert(secd->stat.free_cells == 0,
               "pop_free: free=NIL when nfree=%zd\n", secd->stat.free_cells);
        /* move fixedptr */
        if (secd->fixedptr >= secd->arrayptr)
            return SECD_NIL; /* Out of memory */

        cell = secd->fixedptr;
        ++ secd->fixedptr;
        memdebugf("NEW [%ld] ++\n", cell_index(secd, cell));
    }

    cell->type = CELL_UNDEF;
    cell->nref = 0;
    return cell;
}

int push_free(secd_t *secd, cell_t *c) {
    asserti(c, "push_free(NULL)");
    /*assertv(c->nref == 0,
            "push_free: [%ld]->nref is %ld\n", cell_index(secd, c), (long)c->nref);
            */
    if (c->nref > 0) {
        errorf("push_free: [%ld]->nref is %ld\n",
                cell_index(secd, c), (long)c->nref);
        return -1;
    }
    asserti(c < secd->fixedptr, "push_free: Trying to free array cell");

    if (c + 1 < secd->fixedptr) {
        /* just add the cell to the list secd->free */
        c->type = CELL_FREE;
        c->as.cons.car = SECD_NIL;
        c->as.cons.cdr = secd->free;

        if (not_nil(secd->free))
            secd->free->as.cons.car = c;
        secd->free = c;

        ++secd->stat.free_cells;
        memdebugf("FREE[%ld], %zd free\n",
                cell_index(secd, c), secd->stat.free_cells);
    } else {
        memdebugf("FREE[%ld] --\n", cell_index(secd, c));
        --c;

        while (c->type == CELL_FREE) {
            /* it is a cell adjacent to the free space */
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
                cell_t *next = c->as.cons.cdr;
                if (not_nil(next))
                    next->as.cons.car = SECD_NIL;

                secd->free = next;
            }
            memdebugf("FREE[%ld] --\n", cell_index(secd, c));
            --c;
            --secd->stat.free_cells;
        }

        secd->fixedptr = c + 1;
    }

    return 1;
}

/*
 *      Array memory management
 */

/* checks if the array described by the metadata cons is free */
static inline bool is_array_free(secd_t *secd, cell_t *metacons) {
    if (metacons == secd->arrlist) return false;
    return metacons->as.mcons.free;
}
static inline void mark_free(cell_t *metacons, bool free) {
    metacons->as.mcons.free = free;
}

static cell_t *init_meta(secd_t __unused *secd,
                         cell_t *cell, cell_t *prev, cell_t *next)
{
    cell->type = CELL_ARRMETA;
    cell->nref = 0;
    cell->as.mcons.prev = prev;
    cell->as.mcons.next = next;
    cell->as.mcons.cells = false;
    return cell;
}

cell_t *alloc_array(secd_t *secd, size_t size) {
    /* look through the list of arrays */
    cell_t *cur = secd->arrlist;
    while (not_nil(mcons_next(cur))) {
        if (is_array_free(secd, cur)) {
            size_t cursize = arrmeta_size(secd, cur);
            if (cursize >= size) {
                /* allocate this gap */
                if (cursize > size + 1) {
                    /* make a free gap after */
                    cell_t *newmeta = cur + size + 1;
                    cell_t *prevmeta = mcons_prev(cur);
                    init_meta(secd, newmeta, prevmeta, cur);

                    cur->as.mcons.prev = newmeta;
                    prevmeta->as.mcons.next = newmeta;

                    mark_free(newmeta, true);
                }
                mark_free(cur, false);
                return meta_mem(cur);
            }
        }
        cur = mcons_next(cur);
    }

    /* no chunks of sufficient size found, move secd->arrayptr */
    if (secd->arrayptr - secd->fixedptr <= (int)size)
        return SECD_NIL; /* Out of memory */

    /* create new metadata cons at arrayptr - size - 1 */
    cell_t *oldmeta = secd->arrayptr;

    cell_t *meta = oldmeta - size - 1;
    init_meta(secd, meta, oldmeta, SECD_NIL);

    oldmeta->as.mcons.next = meta;

    secd->arrayptr = meta;

    memdebugf("NEW ARR[%ld], size %zd\n", cell_index(secd, meta), size);
    mark_free(meta, false);
    return meta_mem(meta);
}

void free_array(secd_t *secd, cell_t *mem) {
    assertv(mem <= secd->arrlist, "free_array: tried to free arrlist");
    assertv(secd->arrayptr <= mem, "free_array: not an array");

    cell_t *meta = arr_meta(mem);
    cell_t *prev = mcons_prev(meta);

    assertv(meta->nref == 0, "free_array: someone seems to still use the array");
    mark_free(meta, true);

    if (meta != secd->arrayptr) {
        if (is_array_free(secd, prev)) {
            /* merge with the previous array */
            cell_t *pprev = prev->as.mcons.prev;
            pprev->as.mcons.next = meta;
            meta->as.mcons.prev = pprev;
            prev->type = CELL_UNDEF;
        }

        cell_t *next = mcons_next(meta);
        if (is_array_free(secd, next)) {
            /* merge with the next array */
            cell_t *newprev = meta->as.mcons.prev;
            next->as.mcons.prev = newprev;
            newprev->as.mcons.next = next;
            meta->type = CELL_UNDEF;
        }
    } else {
        /* move arrayptr into the array area */
        prev->as.mcons.next = SECD_NIL;
        secd->arrayptr = prev;

        if (is_array_free(secd, prev)) {
            /* at most one array after 'arr' may be free */
            cell_t *pprev = prev->as.mcons.prev;
            pprev->as.mcons.next = SECD_NIL;
            secd->arrayptr = pprev;
        }
        meta->type = CELL_UNDEF;
    }
    memdebugf("FREE ARR[%ld]", cell_index(secd, meta));
}

void print_array_layout(secd_t *secd) {
    errorf(";; Array heap layout:\n");
    errorf(";;  arrayptr = %ld\n", cell_index(secd, secd->arrayptr));
    errorf(";;  arrlist  = %ld\n", cell_index(secd, secd->arrlist));
    errorf(";; Array list is:\n");
    cell_t *cur = secd->arrlist;
    while (not_nil(mcons_next(cur))) {
        cur = mcons_next(cur);
        errorf(";;  %ld\t%ld (size=%zd,\t%s)\n", cell_index(secd, cur),
                cell_index(secd, mcons_prev(cur)), arrmeta_size(secd, cur),
                (is_array_free(secd, cur)? "free" : "used"));
    }
}

/*
 *      Simple constructors
 */
cell_t *symstore_add(secd_t *secd, const char *str);
cell_t *symstore_lookup(secd_t *secd, const char *str);

static inline cell_t*
init_cons(secd_t *secd, cell_t *cell, cell_t *car, cell_t *cdr) {
    cell->type = CELL_CONS;
    cell->as.cons.car = share_cell(secd, car);
    cell->as.cons.cdr = share_cell(secd, cdr);
    return cell;
}

static inline cell_t *
init_symptr(secd_t __unused *secd, cell_t *cell, const char *str) {
    cell->type = CELL_SYM;
    cell->as.sym.size = strlen(str);

    cell_t *slice = symstore_lookup(secd, str);
    if (cell_type(slice) != CELL_BYTES) {
        slice = symstore_add(secd, str);
    }
    cell->as.sym.bvect = share_cell(secd, slice);
    cell->as.sym.data = slice->as.str.data + slice->as.str.offset;

    return cell;
}

cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr) {
    return init_cons(secd, pop_free(secd), car, cdr);
}

cell_t *new_frame(secd_t *secd, cell_t *syms, cell_t *vals) {
    cell_t *cons = new_cons(secd, syms, vals);
    cons->type = CELL_FRAME;
    /* don't forget to initialize as.frame.io later */
    return cons;
}

cell_t *new_continuation(secd_t *secd, cell_t *s, cell_t *e, cell_t *c) {
    cell_t *k = pop_free(secd);
    k->type = CELL_KONT;
    k->as.kont.stack = share_cell(secd, s);
    k->as.kont.env   = share_cell(secd, e);
    k->as.kont.ctrl  = share_cell(secd, c);
    return k;
}

cell_t *init_number(cell_t *c, int n) {
    c->type = CELL_INT;
    c->as.num = n;
    return c;
}

cell_t *new_number(secd_t *secd, int num) {
    cell_t *cell = pop_free(secd);
    return init_number(cell, num);
}

cell_t *new_char(secd_t *secd, int c) {
    cell_t *cell = pop_free(secd);
    cell->type = CELL_CHAR;
    cell->as.num = c;
    return cell;
}

cell_t *new_symbol(secd_t *secd, const char *sym) {
    cell_t *cell = pop_free(secd);
    return init_symptr(secd, cell, sym);
}

cell_t *new_ref(secd_t *secd, cell_t *to) {
    cell_t *cell = pop_free(secd);
    cell->type = CELL_REF;
    cell->as.ref = share_cell(secd, to);
    return cell;
}

cell_t *new_op(secd_t *secd, opindex_t opind) {
    cell_t *cell = pop_free(secd);
    cell->type = CELL_OP;
    cell->as.op = opind;
    return cell;
}

cell_t *new_array_for(secd_t *secd, cell_t *mem) {
    cell_t *arr = pop_free(secd);
    arr->type = CELL_ARRAY;
    arr->as.arr.data = share_array(secd, mem);
    arr->as.arr.offset = 0;
    return arr;
}

cell_t *new_array(secd_t *secd, size_t size) {
    /* try to allocate memory */
    cell_t *mem = alloc_array(secd, size);
    assert_cell(mem, "new_array: memory allocation failed");
    arr_meta(mem)->as.mcons.cells = true;

    return new_array_for(secd, mem);
}

/*
 *  String allocation
 */
static cell_t *init_strref(secd_t *secd, cell_t *cell, cell_t *mem, size_t size) {
    cell->type = CELL_STR;

    cell->as.str.data = (char *)share_array(secd, mem);
    cell->as.str.offset = 0;
    cell->as.str.size = size;
    return cell;
}

cell_t *new_strref(secd_t *secd, cell_t *mem, size_t size) {
    cell_t *ref = pop_free(secd);
    assert_cell(ref, "new_strref: allocation failed");
    return init_strref(secd, ref, mem, size);
}

cell_t *new_string_of_size(secd_t *secd, size_t size) {
    cell_t *mem;
    mem = alloc_array(secd, bytes_to_cell(size));
    assert_cell(mem, "new_string_of_size: alloc failed");

    return new_strref(secd, mem, size);
}

cell_t *new_string(secd_t *secd, const char *str) {
    size_t size = strlen(str) + 1;
    cell_t *cell = new_string_of_size(secd, size);

    strcpy(strmem(cell), str);
    return cell;
}

cell_t *new_bytevector_of_size(secd_t *secd, size_t size) {
    cell_t *c = new_string_of_size(secd, size);
    c->type = CELL_BYTES;
    return c;
}

/*
 *  Port allocation
 */
static cell_t *init_port_mode(secd_t *secd, cell_t *cell, const char *mode) {
    switch (mode[0]) {
      case 'r':
        cell->as.port.input = true;
        if (mode[1] == '+') {
            cell->as.port.output = true;
            ++mode;
        } else
            cell->as.port.output = false;
        if (mode[1] == '\0')
            return cell;
        break;

      case 'w': case 'a':
        cell->as.port.output = true;
        if (mode[1] == '+') {
            cell->as.port.input = true;
            ++mode;
        } else
            cell->as.port.input = false;
        if (mode[1] == '\0')
            return cell;
    }
    // otherwise fail:
    drop_cell(secd, cell);
    errorf("new_fileport: failed to parse mode\n");
    return new_error(secd, SECD_NIL, "new_port: failed to parse mode");
}

cell_t *new_strport(secd_t *secd, cell_t *str, const char *mode) {
    cell_t *cell = pop_free(secd);
    assert_cell(cell, "new_fileport: allocation failed");

    cell->type = CELL_PORT;
    cell->as.port.file = false;
    cell->as.port.as.str = share_cell(secd, str);
    return init_port_mode(secd, cell, mode);
}

cell_t *new_fileport(secd_t *secd, void *f, const char *mode) {
    cell_t *cell = pop_free(secd);
    assert_cell(cell, "new_fileport: allocation failed");

    cell->type = CELL_PORT;
    cell->as.port.file = true;
    cell->as.port.as.file = f;
    return init_port_mode(secd, cell, mode);
}

/*
 *      Symbol management
 */
const unsigned SYMSTORE_MAX_LOAD_RATIO = 70; // percent

#define SYMSTORE_BUFSIZE  16384        // bytes

enum {
    /* indexes */
    SYMSTORE_HASHSZ = 0,
    SYMSTORE_HASHARR,
    SYMSTORE_BUFLIST,
    /* size */
    SYMSTORE_STRUCT_SIZE
};

static inline const cell_t *
symstore_ref(secd_t *secd, int index) {
    return arr_ref(secd->symstore, index);
}

static inline cell_t *
symstore_get(secd_t *secd, int index) {
    cell_t *c = pop_free(secd);
    return copy_value(secd, c, arr_val(secd->symstore, index));
}

cell_t *symstore_lookup(secd_t *secd, const char *str)
{
    hash_t hash = strhash(str);

    cell_t *arr = share_cell(secd, symstore_get(secd, SYMSTORE_HASHARR));
    size_t hashcap = arr_size(secd, arr);

    size_t ind = hash % hashcap;

    const cell_t *entry = arr_val(arr, ind);
    if (!is_cons(entry))
        return SECD_NIL;

    while (not_nil(entry)) {
        cell_t *slice = list_head(entry);
        const char *hashstr = slice->as.str.data + slice->as.str.offset;
        hash_t entry_hash = ((hash_t*)hashstr)[-1];

        if (entry_hash == hash)
            if (str_eq(hashstr, str))
                return slice;

        entry = list_next(secd, entry);
    }

    return SECD_NIL;
}

void symstorage_ht_insert(secd_t *secd, cell_t *hasharr, hash_t hash, cell_t *val) {
    size_t hashcap = arr_size(secd, hasharr);

    cell_t *hashchain = arr_ref(hasharr, hash % hashcap);
    if (is_cons(hashchain)) {
        cell_t *oldchain = new_clone(secd, hashchain);
        cell_t *newchain = new_cons(secd, val, oldchain);

        drop_value(secd, hashchain);
        copy_value(secd, hashchain, newchain);
    } else {
        init_cons(secd, hashchain, val, SECD_NIL);
    }
}

void symstorage_ht_rebalance(secd_t *secd) {
    cell_t *oldarr = share_cell(secd, symstore_get(secd, SYMSTORE_HASHARR));
    size_t hashcap = arr_size(secd, oldarr);
    size_t newhashcap = 2 * hashcap;

    //errorf(";; symstorage_ht_rebalance to %lu\n", 2 * hashcap);
    cell_t *newarr = share_cell(secd, new_array(secd, newhashcap));

    size_t i;
    for (i = 0; i < hashcap; ++i) {
        cell_t *hashlst = arr_ref(oldarr, i);

        if (!is_cons(hashlst)) continue;

        while (not_nil(hashlst)) {
            cell_t *slice = list_head(hashlst);
            assertv(cell_type(slice) == CELL_BYTES,
                   "symstorage_ht_rebalance: not a bvect slice in the list");
            char *str = slice->as.str.data + slice->as.str.offset;
            hash_t hash = ((hash_t *)str)[-1];

            symstorage_ht_insert(secd, newarr, hash, slice);
            hashlst = list_next(secd, hashlst);
        }
    }

    drop_cell(secd, oldarr);

    arr_set(secd, secd->symstore, SYMSTORE_HASHARR, newarr);
}

cell_t *symstore_add(secd_t *secd, const char *str) {
    size_t hashsize = numval(symstore_ref(secd, SYMSTORE_HASHSZ));

    cell_t *arr = share_cell(secd, symstore_get(secd, SYMSTORE_HASHARR));
    size_t hashcap = arr_size(secd, arr);

    if (((100 * (hashsize + 1)) / hashcap) > SYMSTORE_MAX_LOAD_RATIO) {
        symstorage_ht_rebalance(secd);

        drop_cell(secd, arr);
        arr = share_cell(secd, symstore_get(secd, SYMSTORE_HASHARR));
        hashcap = arr_size(secd, arr);
    }

    off_t bufptr;
    cell_t *buflist = share_cell(secd, symstore_get(secd, SYMSTORE_BUFLIST));
    cell_t *bufbvect;

    if (is_cons(buflist)) {
        bufbvect = list_head(buflist);
        bufptr = bufbvect->as.str.offset;
    } else {
        /* initialize buflist */
        drop_cell(secd, buflist);
        bufbvect = new_bytevector_of_size(secd, SYMSTORE_BUFSIZE);
        buflist = share_cell(secd, new_cons(secd, bufbvect, SECD_NIL));
        bufptr = 0;
    }

    size_t size = strlen(str);
    size_t total_size = sizeof(hash_t) + size + 1;
    assert(total_size < SYMSTORE_BUFSIZE, "Symbol is too large");

    if (bufptr + total_size >= SYMSTORE_BUFSIZE) {
        /* allocate a new buffer then */
        bufbvect = new_bytevector_of_size(secd, SYMSTORE_BUFSIZE);
        buflist = share_cell(secd, new_cons(secd, bufbvect, buflist));
        bufptr = 0;
    }

    /* write the symbol into the buffer */
    hash_t hash = strhash(str);
    off_t symoffset = bufptr + sizeof(hash_t);

    char *bytes = (void *)strmem(bufbvect);
    *(hash_t *)(bytes + bufptr) = hash;
    strcpy(bytes + symoffset, str);

    bufptr += total_size;
    bufbvect->as.str.offset = bufptr;

    cell_t *slice = new_clone(secd, bufbvect);
    slice->as.str.offset = symoffset;

    symstorage_ht_insert(secd, arr, hash, slice);

    /* write hashsize, buflist back */
    arr_ref(secd->symstore, SYMSTORE_HASHSZ)->as.num = hashsize + 1;
    arr_set(secd, secd->symstore, SYMSTORE_BUFLIST, buflist);

    drop_cell(secd, arr); drop_cell(secd, buflist);
    return slice;
}

void init_symstorage(secd_t *secd) {
    secd->symstore = share_cell(secd, new_array(secd, SYMSTORE_STRUCT_SIZE));

    /* hashsize */
    cell_t *hashsize = share_cell(secd, new_number(secd, 0));
    copy_value(secd, arr_ref(secd->symstore, SYMSTORE_HASHSZ), hashsize);

    /* hasharray */
    int inithashcap = 2;    /* initial table capacity */
    cell_t *hasharray = share_cell(secd, new_array(secd, inithashcap));
    copy_value(secd, arr_ref(secd->symstore, SYMSTORE_HASHARR), hasharray);

    /* buflist */
    copy_value(secd, arr_ref(secd->symstore, SYMSTORE_BUFLIST), SECD_NIL);

    drop_cell(secd, hashsize);
    drop_cell(secd, hasharray);
}

/*
 *      Copy constructors
 */
cell_t *copy_value(secd_t *secd,
                   cell_t *__restrict cell,
                   const cell_t *__restrict with)
{
    if (with == SECD_NIL) {
        cell->type = CELL_REF;
        cell->as.ref = SECD_NIL;
        return cell;
    }

    /* copy memory */
    *cell = *with;

    /* a bit of a hack: assign array items nref=1 by default */
    cell->nref = (cell < secd->arrayptr ? 0 : 1);

    switch (cell_type(with)) {
      case CELL_CONS: case CELL_FRAME:
        share_cell(secd, with->as.cons.car);
        share_cell(secd, with->as.cons.cdr);
        break;
      case CELL_KONT:
        share_cell(secd, with->as.kont.stack);
        share_cell(secd, with->as.kont.env);
        share_cell(secd, with->as.kont.ctrl);
        break;
      case CELL_SYM:
        share_cell(secd, with->as.sym.bvect);
        break;
      case CELL_REF:
        share_cell(secd, with->as.ref);
        break;
      case CELL_ARRAY:
        share_array(secd, arr_mem(with));
        break;
      case CELL_STR: case CELL_BYTES:
        share_cell(secd, arr_meta((cell_t *)strmem((cell_t *)with)));
        break;
      case CELL_PORT:
        /* TODO */
        break;
      case CELL_INT: case CELL_CHAR:
      case CELL_OP: case CELL_FUNC:
      case CELL_ERROR: case CELL_UNDEF:
        break;
      case CELL_ARRMETA: case CELL_FREE:
        errorf("copy_value: CELL_ARRMETA/CELL_FREE\n");
        return new_error(secd, SECD_NIL,
                "trying to initialize with CELL_ARRMETA/CELL_FREE");
    }
    return cell;
}

cell_t *new_const_clone(secd_t *secd, const cell_t *from) {
    if (is_nil(from)) return NULL;

    cell_t *clone = pop_free(secd);
    return copy_value(secd, clone, from);
}

cell_t *new_clone(secd_t *secd, cell_t *from) {
    cell_t *clone = pop_free(secd);
    assert_cell(clone, "new_clone: allocation failed");

    return copy_value(secd, clone, from);
}

/*
 *    Error constructors
 */
static cell_t *init_error(secd_t *secd, cell_t *cell, cell_t *info, const char *buf) {
    cell->type = CELL_ERROR;
    cell->as.err.msg = share_cell(secd, new_string(secd, buf));
    cell->as.err.info = share_cell(secd, info);
    cell->as.err.kont = new_continuation(secd, secd->stack, secd->env, secd->control);
    return cell;
}

cell_t *new_errorv(secd_t *secd, cell_t *info, const char *fmt, va_list va) {
#define MAX_ERROR_SIZE  1024
    char buf[MAX_ERROR_SIZE];
    vsnprintf(buf, MAX_ERROR_SIZE, fmt, va);
    return init_error(secd, pop_free(secd), info, buf);
}

cell_t *new_error(secd_t *secd, cell_t *info, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    cell_t *cell = new_errorv(secd, info, fmt, va);
    va_end(va);
    return cell;
}

cell_t *new_error_with(
        secd_t *secd, cell_t *preverr, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    cell_t *err = new_errorv(secd, SECD_NIL, fmt, va);
    va_end(va);

    assign_cell(secd, &err->as.err.info, new_cons(secd, preverr, err->as.err.info));
    return err;
}


/*
 *  Built-in lists manipulation
 */
inline static cell_t *list_push(secd_t *secd, cell_t **to, cell_t *what) {
    cell_t *newtop = new_cons(secd, what, *to);

#if MEMDEBUG
    const char *src;
    if (*to == secd->stack)        src = "S";
    else if (*to == secd->env)     src = "E";
    else if (*to == secd->control) src = "C";
    else if (*to == secd->dump)    src = "D";
    else                           src = "?";
    memdebugf("PUSH %s[%ld (%ld, %ld)]\n", src, cell_index(secd, newtop),
            cell_index(secd, what), cell_index(secd, *to));
#endif

    drop_cell(secd, *to);
    return (*to = share_cell(secd, newtop));
}

inline static cell_t *list_pop(secd_t *secd, cell_t **from) {
    cell_t *top = *from;
    assert(not_nil(top), "pop: stack is empty");
    assert(is_cons(top), "pop: not a cons");

    cell_t *val = share_cell(secd, get_car(top));
    *from = share_cell(secd, get_cdr(top));

#if MEMDEBUG
    const char *src;
    if (*from == secd->stack)        src = "S";
    else if (*from == secd->env)     src = "E";
    else if (*from == secd->control) src = "C";
    else if (*from == secd->dump)    src = "D";
    else                             src = "?";
    memdebugf("POP %s[%ld] (%ld, %ld)\n", src,
            cell_index(secd, top),
            cell_index(secd, val), cell_index(secd, *from));
#endif
    drop_cell(secd, top);
    return val; // don't forget to drop_cell()
}

cell_t *push_stack(secd_t *secd, cell_t *newc) {
    return list_push(secd, &secd->stack, newc);
}

cell_t *pop_stack(secd_t *secd) {
    return list_pop(secd, &secd->stack);
}

cell_t *set_control(secd_t *secd, cell_t **opcons) {
    assert(is_cons(*opcons),
           "set_control: failed, not a cons at [%ld]\n", cell_index(secd, *opcons));
    compile_ctrl(secd, opcons);
    assert_cell(*opcons, "set_control: failed to compile control path");
    assert(cell_type(*opcons) == CELL_CONS, "set_control: not a cons");
    assert(cell_type(get_car(*opcons)) == CELL_OP, "set_control: not an ATOM_OP");
    return assign_cell(secd, &secd->control, *opcons);
}

cell_t *pop_control(secd_t *secd) {
    return list_pop(secd, &secd->control);
}

cell_t *push_dump(secd_t *secd, cell_t *cell) {
    ++secd->stat.used_dump;
    return list_push(secd, &secd->dump, cell);
}

cell_t *pop_dump(secd_t *secd) {
    --secd->stat.used_dump;
    return list_pop(secd, &secd->dump);
}

/*
 *     List/vector/string utilities
 */

size_t list_length(secd_t *secd, cell_t *lst) {
    size_t res = 0;
    while (not_nil(lst)) {
        if (! is_cons(lst))
            break;
        lst = list_next(secd, lst);
        ++res;
    }
    return res;
}

cell_t *fill_array(secd_t *secd, cell_t *arr, cell_t *with) {
    cell_t *data = arr->as.arr.data;
    size_t len = arr_size(secd, arr);
    size_t i;

    for (i = 0; i < len; ++i)
        copy_value(secd, data + i, with);

    return arr;
}

cell_t *list_to_vector(secd_t *secd, cell_t *lst) {
    size_t i;
    size_t len = list_length(secd, lst);
    cell_t *arr = new_array(secd, len);
    assert_cell(arr, "vector_from_list: allocation failed");

    for (i = 0; i < len; ++i) {
        copy_value(secd, arr_ref(arr, i), get_car(lst));
        lst = list_next(secd, lst);
    }
    return arr;
}

cell_t *vector_to_list(secd_t *secd, cell_t *vct, int start, int end) {
    assert(cell_type(vct) == CELL_ARRAY, "vector_to_list: not a vector");
    int len = (int)arr_size(secd, vct);
    assert(start <= len, "vector_to_list: start > len");

    if (end > len) end = len;
    else if (end < 0) end += len;
    if (start < 0) start += len;
    assert(0 <= start && start <= len, "vector_to_list: start is out of range");
    assert(0 <= end && end <= len, "vector_to_list: end is out of range");

    int i;
    cell_t *lst = SECD_NIL;
    cell_t *cur;
    for (i = start; i < end; ++i) {
        cell_t *clone = new_clone(secd, arr_ref(vct, i));
        if (not_nil(lst)) {
            cur->as.cons.cdr = share_cell(secd, new_cons(secd, clone, SECD_NIL));
            cur = list_next(secd, cur);
        } else {
            lst = cur = new_cons(secd, clone, SECD_NIL);
        }
    }
    return lst;
}

/*
 *  Abstract operations
 */
cell_t *fifo_pop(secd_t *secd, cell_t **fifo) {
    switch (cell_type(*fifo)) {
      case CELL_CONS: return list_pop(secd, fifo);
      default: return new_error(secd, SECD_NIL, "fifo_pop: not poppable");
    }
}

cell_t *secd_first(secd_t *secd, cell_t *stream) {
    switch (cell_type(stream)) {
        case CELL_CONS:
            if (not_nil(stream))
                return get_car(stream);
            break;
        case CELL_ARRAY:
            if ((size_t)stream->as.arr.offset < arr_size(secd, stream))
                return new_clone(secd, arr_ref(stream, stream->as.arr.offset));
            break;
        case CELL_STR: {
            const char *mem = strval(stream) + stream->as.str.offset;
            if (mem[0])
                return new_number(secd, (int) utf8get(mem, NULL));
            } break;
        case CELL_BYTES:
            if ((size_t)stream->as.str.offset < mem_size(stream))
                return new_number(secd,
                                 (int) strval(stream)[ stream->as.str.offset ]);
            break;
        default:
            return new_error(secd, SECD_NIL, "first: %s is not iterable",
                                   secd_type_sym(secd, stream));
    }
    /* End-of-stream */
    return SECD_NIL;
}

cell_t *secd_rest(secd_t *secd, cell_t *stream) {
    switch (cell_type(stream)) {
        case CELL_CONS:
            if (not_nil(stream))
                return get_cdr(stream);
            break;
        case CELL_ARRAY:
            if ((size_t)stream->as.arr.offset < arr_size(secd, stream)) {
                cell_t *nxt = new_clone(secd, stream);
                ++nxt->as.arr.offset;
                return nxt;
            }
            break;
        case CELL_STR: {
            const char *mem = strval(stream);
            if (mem[ stream->as.str.offset ]) {
                const char *nxtmem;
                utf8get(mem, &nxtmem);
                cell_t *nxt = new_clone(secd, stream);
                nxt->as.str.offset += (nxtmem - mem);
                return nxt;
            }
            } break;
        case CELL_BYTES:
            if ((size_t)stream->as.str.offset < mem_size(stream)) {
                cell_t *next = new_clone(secd, stream);
                ++next->as.str.offset;
                return next;
            }
            break;
        default:
            return new_error(secd, SECD_NIL, "rest: %s is not iterable",
                                   secd_type_sym(secd, stream));
    }
    return SECD_NIL;
}

/*
 *   Machine-wide operations
 */
void secd_owned_cell_for(cell_t *cell,
        cell_t **ref1, cell_t **ref2, cell_t **ref3)
{
    *ref1 = *ref2 = *ref3 = SECD_NIL;
    switch (cell_type(cell)) {
      case CELL_FRAME:
          *ref3 = cell->as.frame.io;
          /* FALLTHRU */
      case CELL_CONS:
          *ref1 = get_car(cell); *ref2 = get_cdr(cell);
          break;
      case CELL_KONT:
          *ref1 = cell->as.kont.stack;
          *ref2 = cell->as.kont.env;
          *ref3 = cell->as.kont.ctrl;
          break;
      case CELL_STR: case CELL_BYTES:
          *ref1 = arr_meta((cell_t*)strmem(cell));
          break;
      case CELL_ARRAY:
          *ref1 = arr_meta(arr_mem(cell));
          break;
      case CELL_PORT:
          if (!cell->as.port.file)
              *ref1 = cell->as.port.as.str;
          break;
      case CELL_REF:
          *ref1 = cell->as.ref;
          break;
      case CELL_SYM:
          *ref1 = cell->as.sym.bvect;
          break;
      case CELL_ERROR: /* TODO */
      /* getting owned cells is legal, no cells */
      case CELL_INT: case CELL_CHAR: case CELL_OP:
      case CELL_FUNC: case CELL_UNDEF:
          break;
      case CELL_FREE:
      default: break;
    }
}

static cell_t *prepend_index(secd_t *secd, cell_t* hd, cell_t *lst) {
    return new_cons(secd, new_number(secd, cell_index(secd, hd)), lst);
}

cell_t *secd_referers_for(secd_t *secd, cell_t *cell) {
    cell_t *result = SECD_NIL;

    cell_t *ith;
    for (ith = secd->begin; ith < secd->fixedptr; ++ith) {
        cell_t *ref1, *ref2, *ref3;
        secd_owned_cell_for(ith, &ref1, &ref2, &ref3);
        if (ref1 == cell) result = prepend_index(secd, ith, result);
        if (ref2 == cell) result = prepend_index(secd, ith, result);
        if (ref3 == cell) result = prepend_index(secd, ith, result);
    }
    return result;
}

static void increment_nref_for_owned(secd_t *secd, cell_t *cell) {
    if (is_nil(cell)) return;

    ++cell->nref;
    if (cell->nref > 1) return; /* already visited */

    if (cell_type(cell) == CELL_ARRMETA) {
        cell_t *meta = cell;
        if (meta->as.mcons.cells) {
            size_t i;
            size_t len = arrmeta_size(secd, meta);
            for (i = 0; i < len; ++i)
                increment_nref_for_owned(secd, meta_mem(meta) + i);
        }
    } else {
        cell_t *ref1, *ref2, *ref3;
        secd_owned_cell_for(cell, &ref1, &ref2, &ref3);
        if (not_nil(ref1)) increment_nref_for_owned(secd, ref1);
        if (not_nil(ref2)) increment_nref_for_owned(secd, ref2);
        if (not_nil(ref3)) increment_nref_for_owned(secd, ref3);
    }
}

void secd_mark_and_sweep_gc(secd_t *secd) {
    /* set all refcounts to zero */
    cell_t *cell;
    cell_t *meta;

    for (cell = secd->begin; cell < secd->fixedptr; ++cell)
        cell->nref = 0;

    meta = mcons_next(secd->arrlist);
    while (not_nil(meta)) {
        meta->nref = 0;
        if (meta->as.mcons.cells) {
            size_t i;
            size_t len = arrmeta_size(secd, meta);
            for (i = 0; i < len; ++i)
                meta_mem(meta)[i].nref = 0;
        }
        meta = mcons_next(meta);
    }

    /* set new refcounts */
    increment_nref_for_owned(secd, secd->stack);
    increment_nref_for_owned(secd, secd->control);
    increment_nref_for_owned(secd, secd->env);
    increment_nref_for_owned(secd, secd->dump);

    increment_nref_for_owned(secd, secd->debug_port);

    increment_nref_for_owned(secd, secd->symstore);

    /* make new secd->free_list, free unused arrays */
    secd->free = SECD_NIL;
    secd->stat.free_cells = 0;
    for (cell = secd->begin; cell < secd->fixedptr; ++cell) {
        if (cell->nref == 0) {
            if (cell_type(cell) != CELL_FREE) {
                memtracef(";; m&s: cell %ld collected\n",
                          cell_index(secd, cell));
            }

            push_free(secd, cell);
        }
    }

    cell_t *prevmeta = secd->arrlist;
    meta = mcons_next(secd->arrlist);
    while (not_nil(meta)) {
        if (is_array_free(secd, meta) || (meta->nref > 0)) {
            prevmeta = meta;
            meta = mcons_next(meta);
            continue;
        }

        cell_t *pprev = secd->arrlist;
        if (prevmeta != secd->arrlist)
            pprev = mcons_prev(prevmeta);

        /* here prevmeta may disappear: */
        free_array(secd, meta_mem(meta));

        prevmeta = pprev;
        meta = mcons_next(pprev);
    }
}

void init_mem(secd_t *secd, cell_t *heap, size_t size) {
    secd->begin = heap;
    secd->end = heap + size;

    secd->fixedptr = secd->begin;
    secd->arrayptr = secd->end - 1;

    /* init stat */
    secd->stat.used_stack = 0;
    secd->stat.used_dump = 0;
    secd->stat.used_control = 0;
    secd->stat.free_cells = 0;

    /* init array management */
    secd->arrlist = secd->arrayptr;
    init_meta(secd, secd->arrlist, SECD_NIL, SECD_NIL);
    secd->arrlist->nref = DONT_FREE_THIS;

    /* init symbol storage */
    init_symstorage(secd);
}

