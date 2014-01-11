#ifndef __SECD_H__
#define __SECD_H__

#include "conf.h"
#include "debug.h"

#include <stdint.h>
#include <stddef.h>

#define errorf(...) fprintf(stderr, __VA_ARGS__)
#define assert_or_continue(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); fprintf(stderr, "\n"); continue; }
#define assert(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); fprintf(stderr, "\n"); return NULL; }
#define asserti(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); fprintf(stderr, "\n"); return 0; }
#define assertv(cond, ...) \
    if (!(cond)) { errorf(__VA_ARGS__); fprintf(stderr, "\n"); return; }

typedef enum { false, true } bool;

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
    /* main types */
    CELL_UNDEF,
    CELL_CONS,
    /* the same as CELL_CONS */
    CELL_FRAME,  // a environment frame

    CELL_ATOM,
    /* atoms
    CELL_INT,
    CELL_SYM,
    CELL_OP,
    CELL_FUNC, */

    CELL_ERROR,
    //CELL_LAST
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

struct cell {
    // this is a packed structure:
    //      bits 0 .. SECD_ALIGN-1          - enum cell_type
    //      bits SECD_ALIGN .. CHAR_BIT * (sizeof(intptr_t)-1)   - (secd_t *)
    intptr_t type;
    union {
        atom_t  atom;
        cons_t  cons;
        error_t err;
    } as;

    size_t nref;
};

typedef  struct secd_stat  secd_stat_t;

// must be aligned at 1<<SECD_ALIGN
struct secd  {
    cell_t *stack;      // list
    cell_t *env;        // list
    cell_t *control;    // list
    cell_t *dump;       // list

    cell_t *free;       // list
    cell_t *data;       // array
    cell_t *nil;        // pointer

    cell_t *global_env;

    unsigned long tick;

    secd_stream_t *input;

    cell_t *last_list;  // all cells before this are list fixed-size cells

    size_t used_dump;
    size_t used_stack;
    size_t free_cells;
};


/*
 *  Cell accessors
 */

inline static enum cell_type cell_type(const cell_t *c) {
    return ((1 << SECD_ALIGN) - 1) & c->type;
}

inline static secd_t *cell_secd(const cell_t *c) {
    return (secd_t *)((INTPTR_MAX << SECD_ALIGN) & c->type);
}

inline static enum atom_type atom_type(secd_t *secd, const cell_t *c) {
    if (cell_type(c) != CELL_ATOM) return NOT_AN_ATOM;
    return (enum atom_type)(c->as.atom.type);
}

inline static bool is_nil(secd_t *secd, const cell_t *cell) {
    return cell == secd->nil;
}
inline static bool not_nil(secd_t *secd, const cell_t *cell) {
    return !is_nil(secd, cell);
}

inline static long cell_index(secd_t *secd, const cell_t *cons) {
    if (is_nil(secd, cons)) return -1;
    return cons - secd->data;
}

inline static const char * symname(const cell_t *c) {
    return c->as.atom.as.sym.data;
}

inline static int numval(const cell_t *c) {
    return c->as.atom.as.num;
}

void print_cell(secd_t *secd, const cell_t *c);

inline static cell_t *list_next(secd_t *secd, const cell_t *cons) {
    if (cell_type(cons) != CELL_CONS) {
        errorf("list_next: not a cons at [%ld]\n", cell_index(secd, cons));
        print_cell(secd, cons);
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
    return cell_type(cell) == CELL_CONS;
}


#define INIT_SYM(name) {    \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_SYM,     \
      .as.sym = {           \
        .size = DONT_FREE_THIS, \
        .data = (name) } } }

#define INIT_NUM(num) {     \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_INT,     \
      .as.num = (num) }}

#define INIT_OP(op) {     \
    .type = CELL_ATOM,      \
    .nref = DONT_FREE_THIS, \
    .as.atom = {            \
      .type = ATOM_OP,     \
      .as.num = (op) }}

#define INIT_FUNC(func) {  \
    .type = CELL_ATOM,     \
    .nref = DONT_FREE_THIS,\
    .as.atom = {           \
      .type = ATOM_FUNC,   \
      .as.ptr = (func) } }

/*
 * parser
 */
struct secd_stream {
    int (*read)(void *);
    void *state;
};

void print_cell(secd_t *secd, const cell_t *c);
void printc(secd_t *secd, cell_t *c);

void sexp_print(secd_t *secd, cell_t *c);

cell_t *sexp_parse(secd_t *secd, secd_stream_t *f);
cell_t *read_secd(secd_t *secd, secd_stream_t *f);

/*
 * machine
 */

secd_t * init_secd(secd_t *secd, secd_stream_t *readstream);
void run_secd(secd_t *secd, cell_t *ctrl);

#endif //__SECD_H__
