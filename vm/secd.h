#ifndef __SECD_H__
#define __SECD_H__

#include "conf.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#ifndef __unused
# define __unused __attribute__((unused))
#endif

#define errorf(...) secd_errorf(secd, __VA_ARGS__)

/*
 *  Macros that check input not to be SECD_NIL or CELL_ERROR:
 */
#define assert_or_continue(cond, ...) \
    if (!(cond)) \
        { errorf(__VA_ARGS__); errorf("\n"); continue; }

#define assert(cond, ...)  \
    if (!(cond)) {         \
        errorf(__VA_ARGS__); errorf("\n");     \
        return new_error(secd, SECD_NIL, __VA_ARGS__);   }

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
    /* (a&int . b&int . s, e, ADD . c, d) -> (a+b . s, e, c, d) */
    SECD_ADD,

    /* (clos(args, c', e').argv.s, e, AP.c, d)
     *   -> (nil, frame(args, argv).e', c', kont(s.e.c).d)
     * ((kont(s', e', c').d').v.s, e, c, d) -> (v.s', e', c', d')
     */
    SECD_AP,

    /* (clos((arg), c', e').s, e, APCC.c, d)
     *   -> (nil, frame(arg=(kont(s,e,c).d)).e', c', d)
     */
    SECD_APCC,

    /* (v&cons . s, e, CAR.c, d) -> ((car v).s, e, c, d) */
    SECD_CAR,
    /* (v&cons . s, e, CDR.c, d) -> ((cdr v).s, e, c, d) */
    SECD_CDR,
    /* (hd . tl . s, e, CONS.c, d) -> ((hd . tl).s, e, c, d) */
    SECD_CONS,
    /* (x&int . y&int . s, e, DIV.c, d) -> ((x/y).s, e, c, d) */
    SECD_DIV,
    /* (s, e, DUM.c, d) -> (s, dummyframe.e, c, d) */
    SECD_DUM,
    /* (x . y . s, e, EQ.c, d) -> ((eq? x y) . s, e, c, d) */
    SECD_EQ,
    /* (s, e, JOIN.nil, c'.d) -> (s, e, c', d) */
    SECD_JOIN,
    /* (s, e, LD.v.c, d) -> (lookup(v, e).s, e, c, d) */
    SECD_LD,
    /* (s, e, LDC.v.c, d) -> (v.s, e, c, d) */
    SECD_LDC,
    /* (s, e, LDF.(args c').c, d) -> (clos(args, c', e).s, e, c, d) */
    SECD_LDF,
    /* (x&int . y&int . s, e, LEQ.c, d) -> ((x <= y).s, e, c, d) */
    SECD_LEQ,
    /* (x&int . y&int . s, e, MUL.c, d) -> ((x * y).s, e, c, d) */
    SECD_MUL,
    /* (v.s, e, PRINT.c, d) -> (v.s, e, c, d) with priting v to *stdout* */
    SECD_PRN,
    /* (clos(args, c', e').argv.s, e', RAP.c, d)
     *   -> (nil, set-car!(frame(args, argv), e'), c', kont(s, cdr(e'), c).d)
     */
    SECD_RAP,
    /* (s, e, READ.c, d) -> (v.s, e, c, d) where v is read from *stdin* */
    SECD_READ,
    /* (x&int . y&int . s, e, REM.c, d) -> ((x mod y).s, e, c, d) */
    SECD_REM,
    /* (v.nil, e, RTN.nil, kont(s',e',c').d) -> (v.s', e', c', d) */
    SECD_RTN,
    /* (v&bool . s, e, SEL.thenc.elsec.c, d) -> (s, e, (v ? thenc : elsec), c.d) */
    SECD_SEL,
    /* (v.s, e, c, d) -> stop. */
    SECD_STOP,
    /* (x&int . y&int . s, e, SUB.c, d) -> ((x - y).s, e, c, d) */
    SECD_SUB,
    /* (v . s, e, TYPE.c, d) -> (type(v).s, e, c, d) */
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

struct port {
    unsigned char type:3;
    bool input:1;
    bool output:1;
    long data[2];
};

struct error {
    cell_t *info;   // owned object
    cell_t *msg;    // owned string
    cell_t *kont;   // owned cont. or NIL
};

struct string {
    char *data;
    ptrdiff_t offset; // bytes
    size_t size;  // bytes
};

struct array {
    cell_t *data; // array
    ptrdiff_t offset; // cells
};


cell_t *new_error(secd_t *, cell_t *info, const char *fmt, ...);
cell_t *new_errorv(secd_t *secd, cell_t *info, const char *fmt, va_list va);
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
        array_t  arr;           // CELL_ARRAY, CELL_BYTES
        int      num;           // CELL_INT, CELL_CHAR
        void     *ptr;          // CELL_FUNC
        cell_t   *ref;          // CELL_REF
        opindex_t op;           // CELL_OP
        struct metacons mcons;  // CELL_ARRMETA
        struct kont     kont;   // CELL_KONT
    } as;
};

#define SECD_PORTTYPES_MAX  8

typedef  struct portops  portops_t;

typedef enum {
    SECD_NOPOST = 0,
    SECDPOST_GC,
    SECDPOST_MACHINE_DUMP
} secdpostop_t;

typedef struct secd_stat {
    size_t used_stack;
    size_t used_control;
    size_t used_dump;
    size_t free_cells;
    size_t n_alloc;
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
    cell_t *input_port;
    cell_t *output_port;
    cell_t *error_port;
    cell_t *debug_port;
    portops_t* portops[SECD_PORTTYPES_MAX];

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
extern int secd_errorf(secd_t *, const char *, ...);

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
    if (cell_type(c) != CELL_SYM) {
        return NULL;
    }
    return c->as.sym.data;
}
inline static hash_t symhash(const cell_t *c) {
    if (cell_type(c) != CELL_SYM) {
        return 0;
    }
    return ((hash_t *)c->as.sym.data)[-1];
}

inline static int numval(const cell_t *c) {
    return c->as.num;
}
inline static const char *strval(const cell_t *c) {
    switch (cell_type(c)) {
      case CELL_STR: case CELL_BYTES: break;
      default: return NULL;
    }
    return c->as.str.data;
}
inline static char *strmem(cell_t *c) {
    switch (cell_type(c)) {
      case CELL_STR: case CELL_BYTES: break;
      default: return NULL;
    }
    return c->as.str.data;
}

inline static const char * errmsg(const cell_t *err) {
    return strval(err->as.err.msg);
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

#define INIT_FUNC(func) {   \
    .type = CELL_FUNC,      \
    .nref = DONT_FREE_THIS, \
    .as.ptr = (func) }

/*
 * reader/parser
 */

void dbg_print_cell(secd_t *secd, const cell_t *c);
void dbg_printc(secd_t *secd, cell_t *c);

void sexp_print(secd_t *secd, const cell_t *c);
void sexp_pprint(secd_t *secd, cell_t *port, const cell_t *c);
void sexp_display(secd_t *secd, cell_t *port, cell_t *cell);

/* Reads S-expressions from port.
 * If port is SECD_NIL, defaults to secd->input_port */
cell_t *sexp_parse(secd_t *secd, cell_t *port);
cell_t *sexp_lexeme(secd_t *secd, int line, int pos, int prevchar);

cell_t *read_secd(secd_t *secd);

/*
 * machine
 */

secd_t * init_secd(secd_t *secd, cell_t *heap, size_t ncells);
cell_t * run_secd(secd_t *secd, cell_t *ctrl);

/* serialization */
cell_t *serialize_cell(secd_t *secd, cell_t *cell);
cell_t *secd_mem_info(secd_t *secd);

/* control path */
bool is_control_compiled(cell_t *control);
cell_t *compile_control_path(secd_t *secd, cell_t *control);

cell_t *secd_execute(secd_t *secd, cell_t *clos, cell_t *argv);
cell_t *secd_raise(secd_t *secd, cell_t *exc);

/*
 * utilities
 */
hash_t secd_strhash(const char *strz);

const cell_t *secd_default_equal_fun(void);
const cell_t *secd_default_hash_fun(void);

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
