#include "secd.h"
#include "secd_io.h"
#include "memory.h"
#include "env.h"

#include <string.h>
#include <stdint.h>

void print_array_layout(secd_t *secd);

static inline cell_t *to_bool(secd_t *secd, bool cond) {
    return ((cond)? lookup_env(secd, "#t") : SECD_NIL);
}

/*
 *  UTF-8 processing
 *
 *  reference: http://en.wikipedia.org/wiki/UTF-8
 */
size_t utf8len(unichar_t ucs) {
    if (ucs <= 0x7FF)        return 2;
    else if (ucs <= 0xFFFF)  return 3;
    else if (ucs <= 0x10FFF) return 4;
    else                     return 0;
}

char *utf8cpy(char *to, unichar_t ucs) {
    if (ucs < 0x80) {
        *to = (uint8_t)ucs;
        return ++to;
    }

    /* otherwise some bit-twiddling is inevitable */
    int nbytes = utf8len(ucs);
    if (nbytes == 0)
        return NULL;   /* not a valid code point */

    int i = nbytes;
    while (--i > 0) {
        to[i] = 0x80 | (0x3F & (uint8_t)ucs);
        ucs >>= 6;
    }
    uint8_t mask = (1 << (8 - nbytes)) - 1;
    to[0] = ~mask | ((mask >> 1) & (uint8_t)ucs);
    return to + nbytes;
}

size_t utf8strlen(const char *str) {
    size_t result = 0;
    char c;
    while ((c = *str++)) {
        if (c & 0x80) {
            c <<= 1;
            do {
                ++str;
                c <<= 1;
            } while (c & 0x80);
        }
        ++result;
    }
    return result;
}

int utf8taillen(char head, unichar_t *headbits) {
    if ((0xE0 & head) == 0xC0) {
        if (headbits) *headbits = head & 0x1F;
        return 2;
    } else if ((0xF0 & head) == 0xE0) {
        if (headbits) *headbits = head & 0x0F;
        return 3;
    } else if ((0xF8 & head) == 0xF0) {
        if (headbits) *headbits = head & 0x07;
        return 4;
    }
    return 0;
}

unichar_t utf8get(const char *u8, const char **next) {
    if ((0x80 & *u8) == 0) {
        if (next) *next = u8 + 1;
        return *u8;
    }

    unichar_t res;
    int nbytes = utf8taillen(*u8, &res);
    if (nbytes == 0)
        goto decode_error;

    switch (nbytes) {
      case 4:
        ++u8;
        if ((0xC0 & *u8) != 0x80)
            goto decode_error;
        res = (res << 6) | (*u8 & 0x3F);
      case 3:
        ++u8;
        if ((0xC0 & *u8) != 0x80)
            goto decode_error;
        res = (res << 6) | (*u8 & 0x3F);
      case 2:
        ++u8;
        if ((0xC0 & *u8) != 0x80)
            goto decode_error;
        res = (res << 6) | (*u8 & 0x3F);
    }

    if (next) *next = ++u8;
    return res;

decode_error:
    if (next) *next = NULL;
    return 0;
}

/*
 *   List processing
 */

cell_t *secdf_null(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_nullp\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, is_nil(list_head(args)));
}

cell_t *secdf_list(secd_t __unused *secd, cell_t *args) {
    ctrldebugf("secdf_list\n");
    return args;
}

static cell_t *list_copy(secd_t *secd, cell_t *list, cell_t **out_tail) {
    if (is_nil(list))
        return SECD_NIL;

    cell_t *new_head, *new_tail;
    new_head = new_tail = new_cons(secd, list_head(list), SECD_NIL);

    while (not_nil(list = list_next(secd, list))) {
        cell_t *new_cell = new_cons(secd, get_car(list), SECD_NIL);
        new_tail->as.cons.cdr = share_cell(secd, new_cell);
        new_tail = list_next(secd, new_tail);
    }
    if (out_tail)
        *out_tail = new_tail;
    return new_head;
}

cell_t *secdf_copy(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_copy\n");
    return list_copy(secd, list_head(args), NULL);
}

cell_t *secdf_append(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_append\n");
    if (is_nil(args)) return args;
    assert(is_cons(args), "secdf_append: expected arguments");

    cell_t *xs = list_head(args);
    assert(is_cons(list_next(secd, args)), "secdf_append: expected two arguments");

    cell_t *argtail = list_next(secd, args);
    if (is_nil(argtail)) return xs;

    cell_t *ys = list_head(argtail);
    if (not_nil(list_next(secd, argtail))) {
          ys = secdf_append(secd, argtail);
    }

    if (is_nil(xs))
        return ys;

    cell_t *sum = xs;
    cell_t *sum_tail = xs;
    while (true) {
        if (sum_tail->nref > 1) {
            sum_tail = NULL; // xs must be copied
            break;
        }
        if (is_nil(list_next(secd, sum_tail)))
            break;
        sum_tail = list_next(secd, sum_tail);
    }

    if (sum_tail) {
        ctrldebugf("secdf_append: destructive append\n");
        sum_tail->as.cons.cdr = share_cell(secd, ys);
        sum = xs;
    } else {
        ctrldebugf("secdf_append: copying append\n");
        cell_t *sum_tail;
        sum = list_copy(secd, xs, &sum_tail);
        sum_tail->as.cons.cdr = share_cell(secd, ys);
    }

    return sum;
}

/*
 *   Misc native routines
 */

cell_t *secdf_nump(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_nump\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, atom_type(secd, list_head(args)) == ATOM_INT);
}

cell_t *secdf_symp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_symp\n");
    assert(not_nil(args), "secdf_copy: one argument expected");
    return to_bool(secd, atom_type(secd, list_head(args)) == ATOM_SYM);
}

cell_t *secdf_ctl(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_ctl\n");
    if (is_nil(args))
        goto help;

    cell_t *arg1 = list_head(args);
    if (atom_type(secd, arg1) == ATOM_SYM) {
        if (str_eq(symname(arg1), "mem")) {
            printf(";;  size = %zd\n", secd->end - secd->begin);
            printf(";;  fixedptr = %zd\n", secd->fixedptr - secd->begin);
            printf(";;  arrayptr = %zd (%zd)\n",
                    secd->arrayptr - secd->begin, secd->arrayptr - secd->end);
            printf(";;  Fixed cells: %zd free, %zd dump\n",
                    secd->free_cells, secd->used_dump);
            return secd_mem_info(secd);
        } else if (str_eq(symname(arg1), "env")) {
            print_env(secd);
        } else if (str_eq(symname(arg1), "cell")) {
            if (is_nil(list_next(secd, args)))
                goto help;
            cell_t *numc = get_car(list_next(secd, args));
            if (atom_type(secd, numc) != ATOM_INT) {
                printf(";; cell number must be int\n");
                return SECD_NIL;
            }
            int num = numval(numc);
            cell_t *c = secd->begin + num;
            if (c >= secd->end) {
                printf(";; cell number is out of SECD heap\n");
                return SECD_NIL;
            }
            return serialize_cell(secd, c);
        } else if (str_eq(symname(arg1), "where")) {
            if (is_nil(list_next(secd, args)))
                goto help;
            cell_t *c = get_car(list_next(secd, args));
            return new_number(secd, c - secd->begin);
        } else if (str_eq(symname(arg1), "heap")) {
            print_array_layout(secd);
        } else if (str_eq(symname(arg1), "tick")) {
            printf(";; tick = %lu\n", secd->tick);
            return new_number(secd, secd->tick);
        } else {
            goto help;
        }
    }
    return new_symbol(secd, "ok");
help:
    errorf(";; Options are 'tick, 'heap, 'env, 'mem, \n");
    errorf(";;         'where <smth>, 'cell <num>\n");
    errorf(";; Use them like (secd 'env) or (secd 'cell 12)\n");
    errorf(";; If you're here first time, explore (secd 'env)\n");
    errorf(";;    to get some idea of what is available\n");
    return new_symbol(secd, "see?");
}

cell_t *secdf_getenv(secd_t *secd, cell_t __unused *args) {
    ctrldebugf("secdf_getenv\n");
    return secd->env;
}

cell_t *secdf_bind(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_bind\n");

    assert(not_nil(args), "secdf_bind: can't bind nothing to nothing");
    cell_t *sym = list_head(args);
    assert(atom_type(secd, sym) == ATOM_SYM, "secdf_bind: a symbol must be bound");

    args = list_next(secd, args);
    assert(not_nil(args), "secdf_bind: No value for binding");
    cell_t *val = list_head(args);

    cell_t *env;
    // is there the third argument?
    if (not_nil(list_next(secd, args))) {
        args = list_next(secd, args);
        env = list_head(args);
    } else {
        env = secd->global_env;
    }

    cell_t *frame = list_head(env);
    cell_t *old_syms = get_car(frame);
    cell_t *old_vals = get_cdr(frame);

    // an intersting side effect: since there's no check for
    // re-binding an existing symbol, we can create multiple
    // copies of it on the frame, the last added is found
    // during value lookup, but the old ones are persistent
    frame->as.cons.car = share_cell(secd, new_cons(secd, sym, old_syms));
    frame->as.cons.cdr = share_cell(secd, new_cons(secd, val, old_vals));

    drop_cell(secd, old_syms); drop_cell(secd, old_vals);
    return sym;
}

/*
 *     Vector
 */
cell_t *secdv_is(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdv_is: no arguments");
    assert(is_cons(args), "secdv_is: invalid arguments");
    cell_t *obj = get_car(args);
    return to_bool(secd, cell_type(obj) == CELL_ARRAY);
}

cell_t *secdv_make(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdv_make: no arguments");
    assert(is_cons(args), "secdv_make: invalid arguments");

    cell_t *num = get_car(args);
    assert(atom_type(secd, num) == ATOM_INT, "secdv_make: a number expected");

    size_t len = numval(num);
    cell_t *arr = new_array(secd, len);

    if (not_nil(list_next(secd, args))) {
        cell_t *fill = get_car(list_next(secd, args));
        return fill_array(secd, arr, fill);
    } else
        /* make it CELL_UNDEF */
        memset(arr->as.arr.data, 0, sizeof(cell_t) * len);

    return arr;
}

cell_t *secdv_len(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdv_len: no arguments");

    cell_t *vect = get_car(args);
    assert(cell_type(vect) == CELL_ARRAY, "secdv_len: not a vector");

    return new_number(secd, arr_size(secd, vect));
}

cell_t *secdv_ref(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdv_ref: no arguments");

    cell_t *arr = get_car(args);
    assert(cell_type(arr) == CELL_ARRAY, "secdv_ref: array expected");

    args = list_next(secd, args);
    assert(not_nil(args), "secdv_ref: a second argument expected");

    cell_t *num = get_car(args);
    assert(atom_type(secd, num) == ATOM_INT, "secdv_ref: an index expected");
    int ind = numval(num);

    assert(ind < (int)arr_size(secd, arr), "secdv_ref: index is out of range");

    return new_clone(secd, arr->as.arr.data + ind);
}

cell_t *secdv_set(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdv_set: no arguments");

    cell_t *arr = get_car(args);
    assert(cell_type(arr) == CELL_ARRAY, "secdv_set: array expected");

    args = list_next(secd, args);
    assert(not_nil(args), "secdv_set: second argument expected");

    cell_t *num = get_car(args);
    assert(atom_type(secd, num) == ATOM_INT, "secdv_set: an index expected");

    int ind = numval(num);
    assert(ind < (int)arr_size(secd, arr), "secdv_set: index is out of range");

    args = list_next(secd, args);
    assert(not_nil(args), "secdv_set: third argument expected");

    cell_t *obj = get_car(args);
    cell_t *ref = arr->as.arr.data + ind;
    drop_dependencies(secd, ref);
    init_with_copy(secd, ref, obj);

    return arr;
}

cell_t *secdf_vct2lst(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_vct2lst: no arguments");

    cell_t *lst = get_car(args);
    assert(is_cons(lst), "secdf_vct2lst: not a list");

    return vector_from_list(secd, lst);
}

/*
 *    String functions
 */
cell_t *secdf_strp(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_strp: no arguments");

    cell_t *obj = get_car(args);
    return to_bool(secd, cell_type(obj) == CELL_STR);
}

cell_t *secdf_strlen(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_strlen: no arguments");

    cell_t *str = get_car(args);
    assert(cell_type(str) == CELL_STR, "not a string");
    return new_number(secd, utf8strlen((const char *)str->as.str.data));
}

cell_t *secdf_str2sym(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_str2sym: no arguments");

    cell_t *str = get_car(args);
    assert(cell_type(str) == CELL_STR, "not a string");
    return new_symbol(secd, strval(str));
}

cell_t *secdf_sym2str(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_sym2str: no arguments");

    cell_t *sym = get_car(args);
    assert(atom_type(secd, sym) == ATOM_SYM, "not a symbol");
    return new_string(secd, symname(sym));
}

static cell_t *string_to_list(secd_t *secd, const char *cstr) {
    cell_t *res = SECD_NIL;
    cell_t *cur;

    unichar_t codepoint;
    while (1) {
        const char *cnxt = cstr;
        codepoint = utf8get(cstr, &cnxt);

        if (cnxt == NULL) {
            errorf("secdf_str2lst: utf8 decoding failed\n");
            free_cell(secd, res);
            return new_error(secd, "utf8 decoding failed");
        }
        cstr = cnxt;

        if (codepoint == 0)
            return res;

        cell_t *nchr = new_number(secd, codepoint);
        cell_t *ncons = new_cons(secd, nchr, SECD_NIL);
        if (not_nil(res)) {
            cur->as.cons.cdr = share_cell(secd, ncons);
            cur = list_next(secd, cur);
        } else
            res = cur = ncons;
    }
}

static size_t utf8list_len(secd_t *secd, cell_t *lst) {
    size_t strsize = 1;     // for zero
    cell_t *cur = lst;

    /* determine size in bytes */
    while (not_nil(cur)) {
        cell_t *num = get_car(cur);
        if (atom_type(secd, num) != ATOM_INT) {
            errorf("list_to_string: a number expected\n");
            return strsize;
        }

        strsize += utf8len(numval(cur));
        cur = list_next(secd, cur);
    }

    return strsize;
}

static cell_t *list_to_string(secd_t *secd, cell_t *lst) {
    size_t strsize = utf8list_len(secd, lst);
    cell_t *str = new_string_of_size(secd, strsize);
    char *mem = strmem(str); // at least 1 byte

    while (not_nil(lst)) {
        cell_t *num = get_car(lst);
        if (atom_type(secd, num) != ATOM_INT) {
            free_cell(secd, str);
            return new_error(secd, "list_to_string: a number expected");
        }

        mem = utf8cpy(mem, numval(num));
        lst = list_next(secd, lst);
    }

    *mem = '\0';
    return str;
}

cell_t *secdf_str2lst(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_str2lst: no arguments");

    cell_t *str = get_car(args);
    assert(cell_type(str) == CELL_STR, "not a string");

    return string_to_list(secd, strval(str));
}

cell_t *secdf_lst2str(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secd_lst2vct: no arguments");

    cell_t *lst = get_car(args);
    assert(cell_type(lst) == CELL_CONS, "not a cons");

    return list_to_string(secd, lst);
}

/*
 *    I/O ports
 */

/* (open-input-file) */
cell_t *secdf_ifopen(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_open: no arguments");

    cell_t *filename = get_car(args);
    assert(cell_type(filename) == CELL_STR, "secdf_open: a filename string expected");

    return secd_fopen(secd, strval(filename), "r");
}

cell_t *secdf_siopen(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_siopen: no arguments");

    cell_t *str = get_car(args);
    assert(cell_type(str) == CELL_STR, "secdf_siopen: a string is expected");

    return new_strport(secd, str, "r");
}

cell_t *secdf_readstring(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(read-string k): k expected");

    long size;
    cell_t *k = get_car(args);
    assert(cell_type(k), "(read-string): a size number expected");
    size = numval(k);

    args = list_next(secd, args);

    cell_t *port = SECD_NIL;
    if (not_nil(args)) {
        port = get_car(args);
        assert(cell_type(port) == CELL_PORT, "(read-char <port>): port expected");
    } else { // second argument is optional
        port = secd->input_port;
    }

    cell_t *res = new_string_of_size(secd, size);
    assert_cellf(res, "(read-string): failed to allocate string of size %ld", size);

    char *mem = strmem(res);
    if (secd_fread(secd, port, mem, size) > 0)
        return res;
    return new_error(secd, "(read-string): failed to get data");
}

cell_t *secdf_readchar(secd_t *secd, cell_t *args) {
    cell_t *port = SECD_NIL;
    if (not_nil(args)) {
        port = get_car(args);
        assert(cell_type(port) == CELL_PORT, "(read-char <port>): port expected");
    } else {
        port = secd->input_port;
    }

    int b = secd_getc(secd, port);
    if (b == SECD_EOF)
        return new_symbol(secd, EOF_OBJ);

    if (!(b & 0x80))
        return new_number(secd, b);

    unichar_t c;
    int nbytes = utf8taillen(b, &c);
    if (nbytes == 0) {
        errorf("(read-char): not a UTF-8 sequence head\n");
        return new_error(secd, "(read-char): not a UTF-8 sequence head");
    }
    while (--nbytes > 0) {
        b = secd_getc(secd, port);
        if (b == SECD_EOF)
            return new_symbol(secd, EOF_OBJ);
        if ((0xC0 & b) != 0x80) {
            errorf("(read-char): not a UTF-8 sequence at 0x%x\n", b);
            return new_error(secd, "(read-char): not a UTF-8 sequence");
        }
        c = (c << 6) | (0x3F & b);
    }
    return new_number(secd, c);
}

cell_t *secdf_eofp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_eofp\n");
    cell_t *arg1 = list_head(args);
    if (atom_type(secd, arg1) != ATOM_SYM)
        return SECD_NIL;
    return to_bool(secd, str_eq(symname(arg1), EOF_OBJ));
}

cell_t *secdf_pclose(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(port-close): no arguments");

    cell_t *port = get_car(args);
    assert(cell_type(port) == CELL_PORT, "(port-close): a port expected");

    secd_pclose(secd, port);
    return port;
}


/*
 *    Native function mapping table
 */

/* misc */
const cell_t nump_sym   = INIT_SYM("number?");
const cell_t symp_sym   = INIT_SYM("symbol?");
const cell_t eofp_sym   = INIT_SYM("eof-object?");
const cell_t debug_sym  = INIT_SYM("secd");
const cell_t env_sym    = INIT_SYM("interaction-environment");
const cell_t bind_sym   = INIT_SYM("secd-bind!");
/* list routines */
const cell_t list_sym   = INIT_SYM("list");
const cell_t append_sym = INIT_SYM("append");
const cell_t copy_sym   = INIT_SYM("list-copy");
const cell_t nullp_sym  = INIT_SYM("null?");
/* vector routines */
const cell_t vp_sym     = INIT_SYM("vector?");
const cell_t vmake_sym  = INIT_SYM("make-vector");
const cell_t vlen_sym   = INIT_SYM("vector-length");
const cell_t vref_sym   = INIT_SYM("vector-ref");
const cell_t vset_sym   = INIT_SYM("vector-set!");
const cell_t vlist_sym  = INIT_SYM("list->vector");
/* string functions */
const cell_t sp_sym     = INIT_SYM("string?");
const cell_t strlen_sym = INIT_SYM("string-length");
const cell_t symstr_sym = INIT_SYM("symbol->string");
const cell_t strsym_sym = INIT_SYM("string->symbol");
const cell_t strlst_sym = INIT_SYM("string->list");
const cell_t lststr_sym = INIT_SYM("list->string");
/* i/o port functions */
const cell_t fopen_sym  = INIT_SYM("open-input-file");
const cell_t siopen_sym = INIT_SYM("open-input-string");
const cell_t fgetc_sym  = INIT_SYM("read-char");
const cell_t fread_sym  = INIT_SYM("read-string");
const cell_t pclose_sym = INIT_SYM("close-port");

const cell_t list_func  = INIT_FUNC(secdf_list);
const cell_t appnd_func = INIT_FUNC(secdf_append);
const cell_t copy_func  = INIT_FUNC(secdf_copy);
const cell_t nullp_func = INIT_FUNC(secdf_null);
const cell_t nump_func  = INIT_FUNC(secdf_nump);
const cell_t symp_func  = INIT_FUNC(secdf_symp);
const cell_t eofp_func  = INIT_FUNC(secdf_eofp);
const cell_t debug_func = INIT_FUNC(secdf_ctl);
const cell_t getenv_fun = INIT_FUNC(secdf_getenv);
const cell_t bind_func  = INIT_FUNC(secdf_bind);
/* vector routines */
const cell_t vp_func    = INIT_FUNC(secdv_is);
const cell_t vmake_func = INIT_FUNC(secdv_make);
const cell_t vlen_func  = INIT_FUNC(secdv_len);
const cell_t vref_func  = INIT_FUNC(secdv_ref);
const cell_t vset_func  = INIT_FUNC(secdv_set);
const cell_t vlist_func = INIT_FUNC(secdf_vct2lst);
/* string routines */
const cell_t sp_func    = INIT_FUNC(secdf_strp);
const cell_t strlen_fun = INIT_FUNC(secdf_strlen);
const cell_t strsym_fun = INIT_FUNC(secdf_str2sym);
const cell_t symstr_fun = INIT_FUNC(secdf_sym2str);
const cell_t strlst_fun = INIT_FUNC(secdf_str2lst);
const cell_t lststr_fun = INIT_FUNC(secdf_lst2str);
/* i/o ports */
const cell_t fiopen_fun = INIT_FUNC(secdf_ifopen);
const cell_t siopen_fun = INIT_FUNC(secdf_siopen);
const cell_t fgetc_fun  = INIT_FUNC(secdf_readchar);
const cell_t fread_fun  = INIT_FUNC(secdf_readstring);
const cell_t pclose_fun = INIT_FUNC(secdf_pclose);

const cell_t t_sym      = INIT_SYM("#t");
const cell_t f_sym      = INIT_SYM("#f");
const cell_t nil_sym    = INIT_SYM("NIL");

const cell_t err_sym        = INIT_SYM("error:generic");
const cell_t err_nil_sym    = INIT_SYM("error:nil");
const cell_t err_oom        = INIT_SYM("error:out_of_memory");

const struct {
    const cell_t *sym;
    const cell_t *val;
} native_functions[] = {
    // predefined errors
    { &err_oom,     &secd_out_of_memory },
    { &err_nil_sym, &secd_nil_failure },
    { &err_sym,     &secd_failure },

    { &sp_sym,      &sp_func    },
    { &strlen_sym,  &strlen_fun },
    { &symstr_sym,  &symstr_fun },
    { &strsym_sym,  &strsym_fun },
    { &strlst_sym,  &strlst_fun },
    { &lststr_sym,  &lststr_fun },

    { &vp_sym,      &vp_func    },
    { &vmake_sym,   &vmake_func },
    { &vlen_sym,    &vlen_func  },
    { &vref_sym,    &vref_func  },
    { &vset_sym,    &vset_func  },
    { &vlist_sym,   &vlist_func },

    { &fopen_sym,   &fiopen_fun },
    { &siopen_sym,  &siopen_fun },
    { &fgetc_sym,   &fgetc_fun  },
    { &fread_sym,   &fread_fun  },
    { &pclose_sym,  &pclose_fun },

    // native functions
    { &list_sym,    &list_func  },
    { &append_sym,  &appnd_func },
    { &nullp_sym,   &nullp_func },
    { &nump_sym,    &nump_func  },
    { &symp_sym,    &symp_func  },
    { &copy_sym,    &copy_func  },
    { &eofp_sym,    &eofp_func  },
    { &debug_sym,   &debug_func  },
    { &env_sym,     &getenv_fun },
    { &bind_sym,    &bind_func  },

    // symbols
    { &f_sym,       &f_sym      },
    { &t_sym,       &t_sym      },
    { NULL,         NULL        } // must be last
};

cell_t * make_frame_of_natives(secd_t *secd) {
    int i;
    cell_t *symlist = SECD_NIL;
    cell_t *vallist = SECD_NIL;

    for (i = 0; native_functions[i].sym; ++i) {
        cell_t *sym = new_const_clone(secd, native_functions[i].sym);
        cell_t *val = new_const_clone(secd, native_functions[i].val);
        sym->nref = val->nref = DONT_FREE_THIS;
        symlist = new_cons(secd, sym, symlist);
        vallist = new_cons(secd, val, vallist);
    }

    cell_t *sym = new_const_clone(secd, &nil_sym);
    cell_t *val = SECD_NIL;
    symlist = new_cons(secd, sym, symlist);
    vallist = new_cons(secd, val, vallist);

    return new_frame(secd, symlist, vallist);
}
