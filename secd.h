#ifndef __SECD_H__
#define __SECD_H__

/* no includes there */
#include "conf.h"

#include <stdint.h>
#include <stdio.h>

#ifndef __unused
# define __unused __attribute__((unused))
#endif

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

#define SECD_FALSE  "#f"
#define SECD_TRUE   "#t"

typedef  uint32_t       hash_t;

typedef  struct secd    secd_t;
typedef  struct cell    cell_t;

typedef  struct cons  cons_t;
typedef  struct symbol symbol_t;
typedef  struct error error_t;
typedef  struct frame frame_t;
typedef  struct port  port_t;
typedef  struct array array_t;
typedef  struct string string_t;

/* machine operation set */
typedef enum {
    SECD_ADD,   /* (a&int . b&int . s, e, ADD . c, d) -> (a+b . s, e, c, d) */
    SECD_AP,
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
    SECD_TYPE,
    SECD_LAST, // not an operation
} opindex_t;

enum cell_type {
    CELL_UNDEF, // also marks secd->free

    /* compound types */
    CELL_CONS,  // shares two other cells, car and cdr
    CELL_ARRAY, // shares a pointer to cell_t[] in array heap.
    CELL_STR,   // shares a pointer to a UTF8 byte sequence
    CELL_BYTES, // shares a pointer to a raw byte sequence
    CELL_FRAME, // a environment frame, private; the same as CELL_CONS
    CELL_KONT,  // continuation: (stack, env, ctrl)
    CELL_ARRMETA,   // array metadata, private; a double linked node like CELL_CONS
    CELL_FREE,  // free list node; a double linked node like CELL_CONS

    CELL_REF,   // a pivot point between compound and atomic types

    /* atomic types */
    CELL_SYM,
    CELL_INT,
    CELL_CHAR,
    CELL_OP,
    CELL_FUNC,
    CELL_PORT,  // I/O handle

    CELL_ERROR,
};

typedef cell_t* (*secd_opfunc_t)(secd_t *);
typedef cell_t* (*secd_nativefunc_t)(secd_t *, cell_t *);

struct cons {
    cell_t *car;    // shares
    cell_t *cdr;    // shares
};

struct symbol {
    size_t size;
    const char *data;
    cell_t *bvect;
};

struct frame {
    struct cons cons;    // must be first to cast to cons
    cell_t *io;     // cons of *stdin* and *stdout* for the frame
};

struct kont {
    cell_t *stack;
    cell_t *env;
    cell_t *ctrl;
};

struct metacons {
    cell_t *prev;   // prev from arrlist, arrlist-ward
    cell_t *next;   // next from arrlist, arrptr-ward
    bool free:1;    // is area free
    bool cells:1;   // does area contain cells
};

#define SECD_MAXPORT_TYPEBITS   3
#define SECD_MAXPORT_TYPES      (1 << SECD_MAXPORT_TYPEBITS)

typedef  struct secd_port_ops  secd_port_ops_t;

struct port {
    unsigned ptype:SECD_MAXPORT_TYPEBITS;
    bool input:1;
    bool output:1;

    int data[2];
};

struct error {
    size_t len;
    const char *msg; // owns
};

struct string {
    char *data;
    off_t offset; // bytes
    size_t size;  // bytes
};

struct array {
    cell_t *data; // array
    off_t offset; // cells
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

    union {                 // if cell_type is:
        cons_t   cons;          // CELL_CONS, CELL_FREE
        symbol_t sym;           // CELL_SYM
        frame_t  frame;         // CELL_FRAME
        port_t   port;          // CELL_PORT
        error_t  err;           // CELL_ERR
        string_t str;           // CELL_STR
        array_t  arr;           // CELL_ARR, CELL_BYTES
        int      num;           // CELL_INT, CELL_CHAR
        void     *ptr;          // CELL_FUNC
        cell_t   *ref;          // CELL_REF
        opindex_t op;           // CELL_OP
        struct metacons mcons;  // CELL_ARRMETA
        struct kont     kont;   // CELL_KONT
    } as;
};


typedef enum {
    SECD_NOPOST = 0,
    SECDPOST_GC
} secdpostop_t;

typedef struct secd_stat {
    size_t used_stack;
    size_t used_control;
    size_t used_dump;
    size_t free_cells;
} secd_stat_t;

struct secd {
    /**** memory layout ****/
    /* pointers: begin, fixedptr, arrayptr, end
     * - should keep the same position ordering at run-time */
    cell_t *begin;      // the first cell of the heap

    /* these lists reside between secd->begin and secd->fixedptr */
    cell_t *stack;      // list
    cell_t *env;        // list of CELL_FRAME
    cell_t *control;    // list of CELL_OP
    cell_t *dump;       // list of CELL_KONT

    cell_t *free;       // double-linked list
    cell_t *global_env; // frame
    cell_t *symstore;   // symbol storage info array

    // all cells before this one are fixed-size cells
    cell_t *fixedptr;   // pointer

    /* some free space between these two pointers for both to grow in */

    cell_t *arrayptr;   // pointer
    // this one and all cells after are managed memory for arrays

    cell_t *arrlist;    // cdr points to the double-linked list of array metaconses

    cell_t *end;        // the last cell of the heap

    /**** I/O ****/
    secd_port_ops_t * porttypes[ SECD_MAXPORT_TYPES ];

    cell_t *input_port;
    cell_t *output_port;
    cell_t *debug_port;

    /* booleans */
    cell_t *truth_value;
    cell_t *false_value;

    /* how many opcodes executed */
    unsigned long tick;

    /* some operation to be done after the current opcode */
    secdpostop_t postop;

    /* some statistics */
    secd_stat_t stat;
};


/*
 *  Cell accessors
 */

inline static enum cell_type cell_type(const cell_t *c) {
    if (!c) return CELL_CONS;
    return c->type;
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
    return c->as.sym.data;
}
inline static hash_t symhash(const cell_t *c) {
    return ((hash_t *)c->as.sym.data)[-1];
}

inline static const char * errmsg(const cell_t *err) {
    return err->as.err.msg;
}

inline static int numval(const cell_t *c) {
    return c->as.num;
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
inline static bool is_symbol(const cell_t *cell) {
    return cell_type(cell) == CELL_SYM;
}
inline static bool is_number(const cell_t *cell) {
    return cell->type == CELL_INT;
}

inline static bool is_error(const cell_t *cell) {
    if (is_nil(cell)) return false;
    return cell_type(cell) == CELL_ERROR;
}

inline static bool is_input(const cell_t *port) {
    return port->as.port.input;
}
inline static bool is_output(const cell_t *port) {
    return port->as.port.output;
}

inline static cell_t *mcons_prev(cell_t *mcons) {
    return mcons->as.mcons.prev;
}
inline static cell_t *mcons_next(cell_t *mcons) {
    return mcons->as.mcons.next;
}

#define INIT_OP(op) {       \
    .type = CELL_OP,        \
    .nref = DONT_FREE_THIS, \
    .as.num = (op) }

#define INIT_FUNC(func) {   \
    .type = CELL_FUNC,      \
    .nref = DONT_FREE_THIS, \
    .as.ptr = (func) }

#define INIT_ERROR(txt) {   \
    .type = CELL_ERROR,     \
    .nref = DONT_FREE_THIS, \
    .as.err = {             \
        .msg = (txt),       \
        .len = sizeof(txt) } }

/*
 * reader/parser
 */

void dbg_print_cell(secd_t *secd, const cell_t *c);
void dbg_printc(secd_t *secd, cell_t *c);

void sexp_print(secd_t *secd, const cell_t *c);
void sexp_display(secd_t *secd, cell_t *port, cell_t *cell);

/* Reads S-expressions from port.
 * If port is SECD_NIL, defaults to secd->input_port */
cell_t *sexp_parse(secd_t *secd, cell_t *port);

cell_t *read_secd(secd_t *secd);

/*
 * machine
 */

secd_t * init_secd(secd_t *secd);
cell_t * run_secd(secd_t *secd, cell_t *ctrl);

/* serialization */
cell_t *serialize_cell(secd_t *secd, cell_t *cell);
cell_t *secd_mem_info(secd_t *secd);

/* control path */
bool is_control_compiled(cell_t *control);
cell_t *compile_control_path(secd_t *secd, cell_t *control);

/*
 * utilities
 */
hash_t memhash(const char*, size_t);
hash_t strhash(const char *strz);

cell_t *secd_first(secd_t *secd, cell_t *stream);
cell_t *secd_rest(secd_t *secd, cell_t *stream);

/* return a symbol describing the cell */
cell_t *secd_type_sym(secd_t *secd, const cell_t *cell);

/* in the sense of 'equal?' */
bool is_equal(secd_t *secd, const cell_t *a, const cell_t *b);

inline static cell_t *to_bool(secd_t *secd, bool cond) {
    return ((cond)? secd->truth_value : secd->false_value);
}
inline static bool secd_bool(secd_t *secd, cell_t *cell) {
    if (is_symbol(cell) && (is_equal(secd, cell, secd->false_value)))
        return false;
    return true;
}

#endif //__SECD_H__
