#ifndef __SECD_H__
#define __SECD_H__

#include "conf.h"
#include "debug.h"

#include <stdint.h>
#include <stddef.h>

#define errorf(...) fprintf(stderr, __VA_ARGS__)

/*
 *  Macros that check input not to be SECD_NIL or CELL_ERROR:
 */
#define assert_or_continue(cond, ...) \
    if (!(cond)) \
        { errorf(__VA_ARGS__); errorf("\n"); continue; }

#define assert(cond, ...)  \
    if (!(cond)) {         \
        errorf(__VA_ARGS__); errorf("\n");     \
        return new_error(secd, __VA_ARGS__);   }

#define assert_cell(cond, msg)  assert_cellf((cond), "%s", msg)

#define assert_cellf(cond, fmt, ...)  \
    if (is_error(cond)) {             \
        errorf(fmt, __VA_ARGS__); errorf("\n");  \
        return new_error_with(secd, (cond), fmt, __VA_ARGS__); }


#define asserti(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); errorf("\n"); return 0; }

#define assertv(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); errorf("\n"); return; }


#define SECD_NIL   NULL

typedef enum { false, true } bool;

typedef  uint32_t       hash_t;

typedef  struct secd    secd_t;
typedef  struct cell    cell_t;
typedef  long  index_t;

typedef  struct atom  atom_t;
typedef  struct cons  cons_t;
typedef  struct error error_t;

typedef  struct secd_stream  secd_stream_t;

typedef cell_t* (*secd_opfunc_t)(secd_t *);
typedef cell_t* (*secd_nativefunc_t)(secd_t *, cell_t *);

typedef enum {
    SECD_ADD,
    SECD_AP,
    SECD_ATOM,
    SECD_CAR,
    SECD_CDR,
    SECD_CONS,
    SECD_DIV,
    SECD_DUM,
    SECD_EQ,
    SECD_JOIN,
    SECD_LD,
    SECD_LDC,
    SECD_LDF,
    SECD_LEQ,
    SECD_MUL,
    SECD_PRN,
    SECD_RAP,
    SECD_READ,
    SECD_REM,
    SECD_RTN,
    SECD_SEL,
    SECD_STOP,
    SECD_SUB,
    SECD_LAST,
} opindex_t;

enum cell_type {
    CELL_UNDEF, // also marks secd->free

    /* compound types */
    CELL_CONS,  // shares two other cells, car and cdr
    CELL_STR,   // shares a pointer to a UTF8 byte sequence
    CELL_ARRAY, // shares a pointer to cell_t[] in array heap.
    CELL_FRAME, // a environment frame, private; the same as CELL_CONS
    CELL_ARRMETA,   // array metadata, private; a double linked node like CELL_CONS
    CELL_FREE,  // free list node; a double linked node like CELL_CONS

    CELL_REF,   // a pivot point between compound and atomic types

    /* atomic types */
    CELL_ATOM,  // one of commented:
    /* atoms
    CELL_INT,
    CELL_SYM,
    CELL_OP,
    CELL_FUNC, */

    CELL_ERROR,
};

enum atom_type {
    NOT_AN_ATOM,
    ATOM_INT,
    ATOM_SYM,
    ATOM_OP,
    ATOM_FUNC,
};

struct atom {
    enum atom_type type;
    union {
        int num;
        struct {
            size_t size;
            const char *data;
        } sym;

        opindex_t op;
        void *ptr;
    } as;
};

struct cons {
    cell_t *car;    // shares
    cell_t *cdr;    // shares
};

struct error {
    size_t len;
    const char *msg; // owns
};

extern cell_t secd_out_of_memory;
extern cell_t secd_failure;
extern cell_t secd_nil_failure;

cell_t *new_error(secd_t *, const char *fmt, ...);
cell_t *new_errorv(secd_t *secd, const char *fmt, va_list va);
cell_t *new_error_with(secd_t *secd, cell_t *preverr, const char *fmt, ...);

struct cell {
    enum cell_type type:TYPE_BITS;
    size_t nref:NREF_BITS;

    union {
        atom_t  atom;
        cons_t  cons;
        error_t err;
        struct {
            char *data;
            hash_t hash;
        } str;

        cell_t *arr; // array
        cell_t *ref; // pointer
    } as;
};

typedef  struct secd_stat  secd_stat_t;


struct secd {
    /**** memory layout ****/
    /* pointers: begin, fixedptr, arrayptr, end
     * - should keep the same ordering in run-time */
    cell_t *begin;      // the first cell of the heap

    /* these lists reside between secd->begin and secd->fixedptr */
    cell_t *stack;      // list
    cell_t *env;        // list
    cell_t *control;    // list
    cell_t *dump;       // list

    cell_t *free;       // double-linked list
    cell_t *global_env; // frame

    // all cells before this one are fixed-size cells
    cell_t *fixedptr;   // pointer

    /* some free space between these two pointers for both to grow in */

    cell_t *arrlist;    // cdr points to the double-linked list of arrays
    cell_t *arrayptr;   // pointer
    // all cells after this one are managed memory for arrays

    cell_t *end;        // the last cell of the heap

    /**** I/O ****/
    secd_stream_t *input;

    /* some statistics */
    unsigned long tick;

    size_t used_stack;
    size_t used_control;
    size_t used_dump;
    size_t free_cells;
};


/*
 *  Cell accessors
 */

inline static enum cell_type cell_type(const cell_t *c) {
    if (!c) return CELL_CONS;
    return c->type;
}

inline static enum atom_type atom_type(secd_t __unused *secd, const cell_t *c) {
    if (cell_type(c) != CELL_ATOM) return NOT_AN_ATOM;
    return (enum atom_type)(c->as.atom.type);
}

inline static bool is_nil(const cell_t *cell) {
    return cell == SECD_NIL;
}

inline static bool not_nil(const cell_t *cell) {
    return cell != SECD_NIL;
}

inline static long cell_index(secd_t *secd, const cell_t *cons) {
    if (is_nil(cons)) return -1;
    return cons - secd->begin;
}

inline static const char * symname(const cell_t *c) {
    return c->as.atom.as.sym.data;
}

inline static const char * errmsg(const cell_t *err) {
    return err->as.err.msg;
}

inline static int numval(const cell_t *c) {
    return c->as.atom.as.num;
}
inline static const char *strval(const cell_t *c) {
    return c->as.str.data;
}
inline static char *strmem(cell_t *c) {
    return c->as.str.data;
}

void dbg_print_cell(secd_t *secd, const cell_t *c);

inline static cell_t *list_next(secd_t *secd, const cell_t *cons) {
    if (cell_type(cons) != CELL_CONS) {
        errorf("list_next: not a cons at [%ld]\n", cell_index(secd, cons));
        dbg_print_cell(secd, cons);
        return NULL;
    }
    return cons->as.cons.cdr;
}

inline static cell_t *list_head(const cell_t *cons) {
    return cons->as.cons.car;
}

inline static cell_t *get_car(const cell_t *cons) {
    return cons->as.cons.car;
}
inline static cell_t *get_cdr(const cell_t *cons) {
    return cons->as.cons.cdr;
}
inline static bool is_cons(const cell_t *cell) {
    if (is_nil(cell)) return true;
    return cell_type(cell) == CELL_CONS;
}

inline static bool is_error(const cell_t *cell) {
    if (is_nil(cell)) return false;
    return cell_type(cell) == CELL_ERROR;
}

#define INIT_SYM(name) {    \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_SYM,     \
      .as.sym = {           \
        .size = sizeof(name) - 1, \
        .data = (name) } } }

#define INIT_NUM(num) {     \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_INT,     \
      .as.num = (num) }}

#define INIT_OP(op) {       \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_OP,      \
      .as.num = (op) }}

#define INIT_FUNC(func) {   \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_FUNC,    \
      .as.ptr = (func) } }

#define INIT_ERROR(txt) {   \
    .type = CELL_ERROR,     \
    .nref = DONT_FREE_THIS, \
    .as.err = {             \
        .msg = (txt),       \
        .len = sizeof(txt) } }

/*
 * parser
 */
struct secd_stream {
    int (*read)(void *);
    void *state;
};

void dbg_print_cell(secd_t *secd, const cell_t *c);
void dbg_printc(secd_t *secd, cell_t *c);

void sexp_print(secd_t *secd, cell_t *c);

cell_t *sexp_parse(secd_t *secd, secd_stream_t *f);
cell_t *read_secd(secd_t *secd, secd_stream_t *f);

/*
 * machine
 */

secd_t * init_secd(secd_t *secd, secd_stream_t *readstream);
cell_t * run_secd(secd_t *secd, cell_t *ctrl);

/* control path */
bool is_control_compiled(secd_t *secd, cell_t *control);
cell_t *compile_control_path(secd_t *secd, cell_t *control);

/*
 * utilities
 */
hash_t memhash(const char*, size_t);

#endif //__SECD_H__
