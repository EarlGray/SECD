#include "secd.h"
#include "secd_io.h"
#include "memory.h"
#include "env.h"

#include <string.h>

void print_array_layout(secd_t *secd);

/*
 *  UTF-8 processing
 *
 *  reference: http://en.wikipedia.org/wiki/UTF-8
 */

/* takes a codeporint and return its sequence length */
size_t utf8len(unichar_t ucs) {
    if (ucs <= 0x80)         return 1;
    else if (ucs <= 0x7FF)   return 2;
    else if (ucs <= 0xFFFF)  return 3;
    else if (ucs <= 0x10FFF) return 4;
    else                     return 0;
}

/* takes a stream *to, a codepoint ucs, writes codepoint sequence
 * into the stream, returns the next posiiton in the stream */
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

/* takes a byte 'head' representing start of a sequence
 * returns length of the sequence,
 * possibly updating *headbits with meaningful bits of 'head' */
int utf8seqlen(char head, unichar_t *headbits) {
    if ((0x80 & head) == 0) {
        if (headbits) *headbits = head & 0x7F;
        return 1;
    } else if ((0xE0 & head) == 0xC0) {
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

/* takes stream *u8, reads a sequence from it,
 * return codepoint of the sequence,
 * possibly updates **next */
unichar_t utf8get(const char *u8, const char **next) {
    if ((0x80 & *u8) == 0) {
        if (next) *next = u8 + 1;
        return *u8;
    }

    unichar_t res;
    int nbytes = utf8seqlen(*u8, &res);
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

/* returns count of codepoints in the stream until '\0' */
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

/* return pointer to the nth sequence in *str */
const char *utf8nth(const char *str, size_t n) {
    size_t i = 0;
    while ((i++ < n) && str[0]) {
        str += utf8seqlen(str[0], NULL);
    }
    return str;
}

/* return count of bytes in n sequences starting from *str */
size_t utf8memlen(const char *str, size_t n) {
    size_t result = 0;
    size_t i = 0;
    while ((i++ < n) && str[0]) {
        int len = utf8seqlen(str[0], NULL);
        result += len;
        str += len;
    }
    return result;
}

/* copies utf-8 sequences starting at *src
 * until 'bytes' memory is used or more,
 * returns *dst or NULL if there's invalid sequence */
static char *utf8memcpy(char *dst, const char *src, size_t bytes) {
    size_t i = 0;
    while (i < bytes) {
        int n = utf8seqlen(src[i], NULL);
        if (i + n == bytes)
            break;
        if (i + n > bytes)
            return NULL;
        switch (n) {
          case 0: return NULL;
          case 4: dst[i + 3] = src[i + 3];
          case 3: dst[i + 2] = src[i + 2];
          case 2: dst[i + 1] = src[i + 1];
          case 1: dst[i] = src[i];
        }
        i += n;
    }
    return dst;
}

static size_t utf8list_len(secd_t *secd, cell_t *lst) {
    size_t strsize = 1;     // for zero
    cell_t *cur = lst;

    /* determine size in bytes */
    while (not_nil(cur)) {
        cell_t *num = get_car(cur);
        if (! (is_number(num) || cell_type(num) == CELL_CHAR)) {
            errorf("list_to_string: a number expected\n");
            return strsize;
        }

        strsize += utf8len(numval(cur));
        cur = list_next(secd, cur);
    }

    return strsize;
}

/*
 *   List processing
 */

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

static bool get_two_nums(secd_t *secd, cell_t *lst,
        size_t *fst, size_t *snd,
        const char *signiture)
{
    lst = list_next(secd, lst);
    if (is_nil(lst)) return true;

    cell_t *startc = get_car(lst);
    asserti(is_number(startc), "%s: an int expected as second argument", signiture);
    *fst = numval(startc);

    lst = list_next(secd, lst);
    if (is_nil(lst)) return true;

    cell_t *endc = get_car(lst);
    asserti(is_number(endc), "%s: an int expected as third argument", signiture);
    *snd = numval(endc);

    return true;
}

cell_t *secdf_defp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_defp\n");
    assert(not_nil(args), "secdf_defp: no arguments");

    cell_t *sym = list_head(args);
    assert(is_symbol(sym), "secdf_deps: not a symbol");

    cell_t *defc = SECD_NIL;
    lookup_env(secd, symname(sym), &defc);
    return to_bool(secd, not_nil(defc));
}

cell_t *secdf_hash(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_hash\n");
    assert(not_nil(args), "secdf_hash: no arguments");

    cell_t *cell = list_head(args);
    if (is_symbol(cell))
        return new_number(secd, symhash(cell));
    return SECD_NIL;
}

cell_t *secdf_ctl(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_ctl\n");
    if (is_nil(args))
        goto help;

    cell_t *arg1 = list_head(args);
    if (is_symbol(arg1)) {
        if (str_eq(symname(arg1), "mem")) {
            secd_printf(secd, ";;  size = %zd\n", secd->end - secd->begin);
            secd_printf(secd, ";;  fixedptr = %zd\n", secd->fixedptr - secd->begin);
            secd_printf(secd, ";;  arrayptr = %zd (%zd)\n",
                         secd->arrayptr - secd->begin, secd->arrayptr - secd->end);
            secd_printf(secd, ";;  Fixed cells: %zd free\n", secd->stat.free_cells);
            return secd_mem_info(secd);
        } else if (str_eq(symname(arg1), "env")) {
            print_env(secd);
        } else if (str_eq(symname(arg1), "dump")) {
            secd->postop = SECDPOST_MACHINE_DUMP;
            secd_printf(secd, ";; Dump of the machine written to secdstate.dump\n");
        } else if (str_eq(symname(arg1), "viewdump")) {
            cell_t *dlist = SECD_NIL;
            cell_t *lstcur = SECD_NIL;
            cell_t *dmpcur;
            for (dmpcur = secd->dump;
                 not_nil(dmpcur);
                 dmpcur = list_next(secd, dmpcur))
            {
                cell_t *cns = new_cons(secd,
                        new_number(secd, cell_index(secd, dmpcur)), SECD_NIL);
                if (not_nil(dlist)) {
                    lstcur->as.cons.cdr = share_cell(secd, cns);
                    lstcur = cns;
                } else {
                    dlist = lstcur = cns;
                }
            }
            return dlist;
        } else if (str_eq(symname(arg1), "cell")) {
            if (is_nil(list_next(secd, args)))
                goto help;
            cell_t *numc = get_car(list_next(secd, args));
            if (! is_number(numc)) {
                secd_printf(secd, ";; cell number must be int\n");
                return SECD_NIL;
            }
            int num = numval(numc);
            cell_t *c = secd->begin + num;
            if (c >= secd->end) {
                secd_printf(secd, ";; cell number is out of SECD heap\n");
                return SECD_NIL;
            }
            cell_t *ret = serialize_cell(secd, c);
            if (cell_type(c) == CELL_ARRMETA)
                secd_pdump_array(secd, secd->output_port, c);
            return ret;
        } else if (str_eq(symname(arg1), "where")) {
            if (is_nil(list_next(secd, args)))
                goto help;
            cell_t *c = get_car(list_next(secd, args));
            return new_number(secd, c - secd->begin);
        } else if (str_eq(symname(arg1), "owner")) {
            cell_t *numc = get_car(list_next(secd, args));
            return secd_referers_for(secd, secd->begin + numval(numc));
        } else if (str_eq(symname(arg1), "heap")) {
            print_array_layout(secd);
        } else if (str_eq(symname(arg1), "gc")) {
            secd->postop = SECDPOST_GC;
        } else if (str_eq(symname(arg1), "tick")) {
            secd_printf(secd, ";; tick = %lu\n", secd->tick);
            return new_number(secd, secd->tick);
        } else if (str_eq(symname(arg1), "state")) {
            secd_printf(secd, ";; stack = %ld\n", cell_index(secd, secd->stack));
            secd_printf(secd, ";; env   = %ld\n", cell_index(secd, secd->env));
            secd_printf(secd, ";; ctrl  = %ld\n", cell_index(secd, secd->control));
            secd_printf(secd, ";; dump  = %ld\n\n", cell_index(secd, secd->dump));
            secd_printf(secd, ";; %s = %ld\n", 
                    SECD_TRUE,  cell_index(secd, secd->truth_value));
            secd_printf(secd, ";; %s = %ld\n\n",
                    SECD_FALSE, cell_index(secd, secd->false_value));
            secd_printf(secd, ";; *stdin*  = %ld\n",
                    cell_index(secd, secd->input_port));
            secd_printf(secd, ";; *stdout* = %ld\n",
                    cell_index(secd, secd->output_port));
            secd_printf(secd, ";; *stddbg* = %ld\n\n",
                    cell_index(secd, secd->debug_port));
        } else {
            goto help;
        }
    }
    return new_symbol(secd, "ok");
help:
    errorf(";; Options are 'env, 'mem, 'heap,\n");
    errorf(";;    'tick, 'dump, 'state, 'viewdump, 'gc, \n");
    errorf(";;    'where <smth>, 'cell <num>, 'owner <num>\n");
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
    assert(is_symbol(sym), "secdf_bind: a symbol must be bound");

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

    secd_insert_in_frame(secd, list_head(env), sym, val);
    return sym;
}

cell_t *secdf_chrint(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_chrint: no argument");
    cell_t *chr = get_car(args);
    assert(cell_type(chr) == CELL_CHAR, "secdf_chrint: not a character");
    return new_number(secd, numval(chr));
}

cell_t *secdf_intchr(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_intchr: no arguments");
    cell_t *num = get_car(args);
    assert(is_number(num), "secdf_intchr: not a number");
    return new_char(secd, numval(num));
}

cell_t *secdf_xor(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(xor): no arguments");
    assert(is_cons(args), "(xor): invalid arguments");

    cell_t *num1 = get_car(args);
    assert(is_number(num1), "(xor): num1 not a number");

    assert(not_nil(args), "(xor): no num2");
    args = list_next(secd, args);

    cell_t *num2 = get_car(args);
    assert(is_number(num2), "(xor): num2 not a number");

    int res = numval(num1) ^ numval(num2);
    cell_t *resc = new_number(secd, res);
    return resc;
}

cell_t *secdf_or(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(or): no arguments");
    assert(is_cons(args), "(or): invalid arguments");

    cell_t *num1 = get_car(args);
    assert(is_number(num1), "(or): num1 not a number");

    assert(not_nil(args), "(or): no num2");
    args = list_next(secd, args);

    cell_t *num2 = get_car(args);
    assert(is_number(num2), "(or): num2 not a number");

    int res = numval(num1) | numval(num2);
    cell_t *resc = new_number(secd, res);
    return resc;
}

/*
 *     Vector
 */
cell_t *secdv_make(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdv_make: no arguments");
    assert(is_cons(args), "secdv_make: invalid arguments");

    cell_t *num = get_car(args);
    assert(is_number(num), "secdv_make: a number expected");

    size_t len = numval(num);
    cell_t *arr = new_array(secd, len);

    if (not_nil(list_next(secd, args))) {
        cell_t *fill = get_car(list_next(secd, args));
        return fill_array(secd, arr, fill);
    } else
        /* make it CELL_UNDEF */
        memset((char *)arr_ref(arr, 0), CELL_UNDEF, sizeof(cell_t) * len);

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
    assert(is_number(num), "secdv_ref: an index expected");
    int ind = numval(num);

    assert(ind < (int)arr_size(secd, arr), "secdv_ref: index is out of range");

    return new_clone(secd, arr_ref(arr, ind));
}

cell_t *secdv_set(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdv_set: no arguments");

    cell_t *arr = get_car(args);
    assert(cell_type(arr) == CELL_ARRAY, "secdv_set: array expected");

    args = list_next(secd, args);
    assert(not_nil(args), "secdv_set: second argument expected");

    cell_t *num = get_car(args);
    assert(is_number(num), "secdv_set: an index expected");

    int ind = numval(num);
    assert(ind < (int)arr_size(secd, arr), "secdv_set: index is out of range");

    args = list_next(secd, args);
    assert(not_nil(args), "secdv_set: third argument expected");

    cell_t *obj = get_car(args);

    return arr_set(secd, arr, ind, obj);
}

cell_t *secdf_lst2vct(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_lst2vct: no arguments");

    cell_t *lst = get_car(args);
    assert(is_cons(lst), "secdf_lst2vct: not a list");

    return list_to_vector(secd, lst);
}

cell_t *secdf_vct2lst(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_vct2lst: no arguments");

    cell_t *vct = get_car(args);
    assert(cell_type(vct) == CELL_ARRAY, "secdf_vct2lst: not an array");

    size_t start = 0;
    size_t end = (int)arr_size(secd, vct);
    get_two_nums(secd, args, &start, &end, "vct2lst");

    return vector_to_list(secd, vct, start, end);
}

/*
 *    String functions
 */
cell_t *secdf_strlen(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_strlen: no arguments");

    cell_t *str = get_car(args);
    assert(cell_type(str) == CELL_STR, "not a string");
    return new_number(secd, utf8strlen((const char *)str->as.str.data));
}

cell_t *secdf_strref(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_strref: no arguments");

    cell_t *str = get_car(args);
    assert(cell_type(str) == CELL_STR, "secdf_strref: not a string");

    args = list_next(secd, args);
    cell_t *numc = get_car(args);

    assert(is_number(numc), "secdf_strref: a number expected");
    const char *nthptr = utf8nth(strmem(str), numval(numc));
    assert(*nthptr, "secdf_strref: index out of range");

    unichar_t c = utf8get(nthptr, NULL);
    return new_char(secd, c);
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
    assert(is_symbol(sym), "secdf_sym2str: not a symbol");
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
            free_cell(secd, res);
            errorf("secdf_str2lst: utf8 decoding failed\n");
            return new_error(secd, SECD_NIL, "str->lst: utf8 decoding failed");
        }
        cstr = cnxt;

        if (codepoint == 0)
            return res;

        cell_t *nchr = new_char(secd, codepoint);
        cell_t *ncons = new_cons(secd, nchr, SECD_NIL);
        if (not_nil(res)) {
            cur->as.cons.cdr = share_cell(secd, ncons);
            cur = list_next(secd, cur);
        } else
            res = cur = ncons;
    }
}

static cell_t *list_to_string(secd_t *secd, cell_t *lst) {
    size_t strsize = utf8list_len(secd, lst);
    cell_t *str = new_string_of_size(secd, strsize);
    char *mem = strmem(str); // at least 1 byte

    while (not_nil(lst)) {
        cell_t *num = get_car(lst);
        if (cell_type(num) != CELL_CHAR) {
            free_cell(secd, str);
            return new_error(secd, SECD_NIL, "list_to_string: a number expected");
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
 *    Bytevector utilities
 */
cell_t *secdf_mkbvect(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_mkbvect: no arguments");

    cell_t *numc = get_car(args);
    assert(is_number(numc), "secdf_mkbvect: not a number");

    cell_t *bv = new_bytevector_of_size(secd, numval(numc));
    assert_cell(bv, "secdf_mkbvect: failed to allocate");

    args = list_next(secd, args);
    if (not_nil(args)) {
        cell_t *numc = get_car(args);
        assert(is_number(numc), "secdf_mkbvect: fill value must be an int");
        memset(strmem(bv), numval(numc), mem_size(bv));
    }
    return bv;
}

cell_t *secdf_bvlen(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_bvlen: no aruments");

    cell_t *bv = get_car(args);
    assert(cell_type(bv) == CELL_BYTES, "secdf_bvlen: not a bytevector");

    return new_number(secd, mem_size(bv));
}

cell_t *secdf_bvref(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_bvref: no arguments");

    cell_t *bv = get_car(args);
    assert(cell_type(bv) == CELL_BYTES, "secdf_bvref: not a bytevector");

    args = list_next(secd, args);
    assert(not_nil(args), "secdf_bvref: second argument expected");

    cell_t *numc = get_car(args);
    assert(is_number(numc), "secdf_bvref: index must be a nummber");

    size_t n = numval(numc);
    assert(n < mem_size(bv), "secdf_bvref: out of range");

    return new_number(secd, (unsigned char)strval(bv)[ n ]);
}

/*
cell_t *secdf_bvcopy(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_bvcopy: no arguments, must be (bytevector-copy! to at from [start [end]])");

    cell_t *to = get_car(args);
    assert(cell_type(to) == CELL_BYTES, "secdf_bvcopy: first argument is not a bytevector");

    args = list_next(secd, args);
    assert(not_nil(args), "secdf_bvcopy: second argument expected");
    cell_t *atc = get_car(args);
    assert(is_number(atc), "secdf_bvcopy: second argument must be index of the first bytevector");
    size_t at = numval(atc);
    assert(at < mem_size(to), "secdf_bvcopy: at is out of bounds");

    args = list_next(secd, args);
    assert(not_nil(args), "secdf_bvcopy: third argument expected");
    cell_t *from = get_car(args);
    assert(cell_type(from) == CELL_BYTES, "secdf_bvcopy: third argument is not a bytevector");

    size_t start = 0, end = -1;
    get_two_nums(secd, args, &start, &end, "secdf_bvcopy");

}
*/

cell_t *secdf_bvset(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_bvset: no arguments");

    cell_t *bv = get_car(args);
    assert(cell_type(bv) == CELL_BYTES, "secdf_bvset: not a bytevector");

    args = list_next(secd, args);
    assert(not_nil(args), "secdf_bvset: second argument expected");

    cell_t *numc = get_car(args);
    assert(is_number(numc), "secdf_bvset: second argument must be integer");
    size_t n = numval(numc);
    assert(n < mem_size(bv), "secdf_bvset: index out of range");

    args = list_next(secd, args);
    assert(not_nil(args), "secdf_bvset: third argument expected");
    cell_t *valc = get_car(args);
    assert(is_number(valc), "secdf_bvset: must be a number");

    strmem(bv)[ n ] = numval(valc);
    return bv;
}

cell_t *secdf_bv2str(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "bv2str: no arguments");

    cell_t *bv = list_head(args);
    assert(cell_type(bv) == CELL_BYTES, "bv2str: not a bytevector");

    size_t start = 0, end = mem_size(bv);
    get_two_nums(secd, args, &start, &end, "bv2str");

    cell_t *str = new_string_of_size(secd, end - start);
    if (utf8memcpy(strmem(str), strval(bv) + start, end - start))
        return str;

    free_cell(secd, str);
    return secd->false_value;
}

cell_t *secdf_str2bv(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "str2bv: no arguments");

    cell_t *str = list_head(args);
    assert(cell_type(str) == CELL_STR, "str2bv: not a string");

    size_t start = 0, end = utf8strlen(strval(str)) + 1;
    get_two_nums(secd, args, &start, &end, "str2bv");
    /* TODO: check ranges */

    const char *startmem = utf8nth(strval(str), start);
    size_t len = utf8memlen(startmem, end - start) + 1;
    cell_t *bv = new_bytevector_of_size(secd, len);

    utf8memcpy(strmem(bv), startmem, len);
    return bv;
}


/*
 *    I/O ports
 */

cell_t *secdf_display(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "secdf_diplay: no arguments");

    cell_t *what = get_car(args);

    args = list_next(secd, args);
    cell_t *port = secd->output_port;
    if (not_nil(args)) {
        cell_t *p = get_car(args);
        assert(cell_type(p) == CELL_PORT,
                "secdf_display: second argument is not a port");
        port = p;
    }

    sexp_display(secd, port, what);
    return SECD_NIL;
}

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

    cell_t *k = get_car(args);
    assert(cell_type(k), "(read-string): a size number expected");
    long size = numval(k);

    args = list_next(secd, args);

    cell_t *port = SECD_NIL;
    if (not_nil(args)) {
        port = get_car(args);
        assert(cell_type(port) == CELL_PORT, "(read-char <port>): port expected");
    } else { // second argument is optional
        port = secd->input_port;
    }

    /* TODO: caveat: k is length of a UTF-8 sequence */
    cell_t *res = new_string_of_size(secd, size);
    assert_cellf(res, "(read-string): failed to allocate string of size %ld", size);

    char *mem = strmem(res);
    if (secd_fread(secd, port, mem, size) > 0)
        return res;
    return new_error(secd, SECD_NIL, "(read-string): failed to get data");
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
        return new_char(secd, b);

    unichar_t c;
    int nbytes = utf8seqlen(b, &c);
    if (nbytes == 0) {
        errorf("(read-char): not a UTF-8 sequence head\n");
        return new_error(secd, SECD_NIL, "(read-char): not a UTF-8 sequence head");
    }
    while (--nbytes > 0) {
        b = secd_getc(secd, port);
        if (b == SECD_EOF)
            return new_symbol(secd, EOF_OBJ);
        if ((0xC0 & b) != 0x80) {
            errorf("(read-char): not a UTF-8 sequence at 0x%x\n", b);
            return new_error(secd, SECD_NIL, "(read-char): not a UTF-8 sequence");
        }
        c = (c << 6) | (0x3F & b);
    }
    return new_char(secd, c);
}

cell_t *secdf_readu8(secd_t *secd, cell_t *args) {
    cell_t *port = SECD_NIL;
    if (not_nil(args)) {
        port = get_car(args);
        assert(cell_type(port) == CELL_PORT, "(read-u8 <port>): port expected");
    } else {
        port = secd->input_port;
    }

    int b = secd_getc(secd, port);
    if (b == SECD_EOF)
        return new_symbol(secd, EOF_OBJ);
    return new_number(secd, b);
}

cell_t *secdf_eofp(secd_t *secd, cell_t *args) {
    ctrldebugf("secdf_eofp\n");
    cell_t *arg1 = list_head(args);
    if (!is_symbol(arg1))
        return secd->false_value;
    return to_bool(secd, str_eq(symname(arg1), EOF_OBJ));
}

cell_t *secdf_pinfo(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(port-info): no arguments)");

    cell_t *port = get_car(args);
    assert(cell_type(port) == CELL_PORT, "(port-info): not a port");

    cell_t *inp = new_cons(secd, new_symbol(secd, "in"),
                                 to_bool(secd, port->as.port.input));
    cell_t *outp = new_cons(secd, new_symbol(secd, "out"),
                                  to_bool(secd, port->as.port.output));
    cell_t *binp = new_cons(secd, new_symbol(secd, "txt"),
                                    secd->truth_value);
    cell_t *filep = new_cons(secd, new_symbol(secd, "file"),
                                   to_bool(secd, port->as.port.file));
    cell_t *info =
        new_cons(secd, inp,
          new_cons(secd, outp,
             new_cons(secd, binp,
                 new_cons(secd, filep, SECD_NIL))));
    return info;
}

cell_t *secdf_pclose(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(port-close): no arguments");

    cell_t *port = get_car(args);
    assert(cell_type(port) == CELL_PORT, "(port-close): a port expected");

    secd_pclose(secd, port);
    return port;
}


cell_t *secdf_raise(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(raise): no argument");

    cell_t *info = get_car(args);
    cell_t *err = new_error(secd, info, "raised");

    return secd_raise(secd, err);
}

cell_t *secdf_raisec(secd_t *secd, cell_t *args) {
    assert(not_nil(args), "(raise-continuable): no arguments");

    /* TODO */
    return SECD_NIL;
}


/*
 *    Native function mapping table
 */

const cell_t defp_func  = INIT_FUNC(secdf_defp);
const cell_t eofp_func  = INIT_FUNC(secdf_eofp);
const cell_t debug_func = INIT_FUNC(secdf_ctl);
const cell_t getenv_fun = INIT_FUNC(secdf_getenv);
const cell_t bind_func  = INIT_FUNC(secdf_bind);
const cell_t hash_func  = INIT_FUNC(secdf_hash);

//const cell_t strnum_fun = INIT_FUNC(secdf_str2num);
//const cell_t numstr_fun = INIT_FUNC(secdf_num2str);
const cell_t xorint_fun = INIT_FUNC(secdf_xor);
const cell_t orint_fun  = INIT_FUNC(secdf_or);

const cell_t raise_fun  = INIT_FUNC(secdf_raise);
const cell_t raisec_fun = INIT_FUNC(secdf_raisec);
/* list functions */
const cell_t list_func  = INIT_FUNC(secdf_list);
const cell_t appnd_func = INIT_FUNC(secdf_append);
/* char functions  */
const cell_t chrint_fun = INIT_FUNC(secdf_chrint);
const cell_t intchr_fun = INIT_FUNC(secdf_intchr);
/* string routines */
const cell_t strlen_fun = INIT_FUNC(secdf_strlen);
const cell_t strref_fun = INIT_FUNC(secdf_strref);
const cell_t strsym_fun = INIT_FUNC(secdf_str2sym);
const cell_t symstr_fun = INIT_FUNC(secdf_sym2str);
const cell_t strlst_fun = INIT_FUNC(secdf_str2lst);
const cell_t lststr_fun = INIT_FUNC(secdf_lst2str);
/* vector routines */
const cell_t vmake_func = INIT_FUNC(secdv_make);
const cell_t vlen_func  = INIT_FUNC(secdv_len);
const cell_t vref_func  = INIT_FUNC(secdv_ref);
const cell_t vset_func  = INIT_FUNC(secdv_set);
const cell_t vlist_func = INIT_FUNC(secdf_vct2lst);
const cell_t l2v_func   = INIT_FUNC(secdf_lst2vct);
/* bytevectors */
const cell_t mkbv_fun   = INIT_FUNC(secdf_mkbvect);
const cell_t bvlen_fun  = INIT_FUNC(secdf_bvlen);
const cell_t bvref_fun  = INIT_FUNC(secdf_bvref);
//const cell_t bvcopy_fun = INIT_FUNC(secdf_bvcopy);
const cell_t bvset_fun  = INIT_FUNC(secdf_bvset);
const cell_t bv2str_fun = INIT_FUNC(secdf_bv2str);
const cell_t str2bv_fun = INIT_FUNC(secdf_str2bv);
/* i/o ports */
const cell_t displ_fun  = INIT_FUNC(secdf_display);
const cell_t fiopen_fun = INIT_FUNC(secdf_ifopen);
const cell_t siopen_fun = INIT_FUNC(secdf_siopen);
const cell_t fgetc_fun  = INIT_FUNC(secdf_readchar);
const cell_t fread_fun  = INIT_FUNC(secdf_readstring);
const cell_t freadb_fun = INIT_FUNC(secdf_readu8);
//const cell_t readln_fun = INIT_FUNC(secdf_readln);
const cell_t pinfo_fun  = INIT_FUNC(secdf_pinfo);
const cell_t pclose_fun = INIT_FUNC(secdf_pclose);

const native_binding_t
native_functions[] = {
    { "string-length",  &strlen_fun },
    { "string-ref",     &strref_fun },
    { "symbol->string", &symstr_fun },
    { "string->symbol", &strsym_fun },
    { "string->list",   &strlst_fun },
    { "list->string",   &lststr_fun },
    //{ "string->number", &strnum_fun },
    //{ "number->string", &numstr_fun },

    { "int-xor",        &xorint_fun },
    { "int-or",         &orint_fun  },

    { "char->integer",  &chrint_fun },
    { "integer->char",  &intchr_fun },

    { "make-bytevector",    &mkbv_fun  },
    { "bytevector-length",  &bvlen_fun },
    //{ "bytevector-copy!",   &bvcopy_fun },
    { "bytevector-u8-ref",  &bvref_fun  },
    { "bytevector-u8-set!", &bvset_fun  },
    { "utf8->string",       &bv2str_fun },
    { "string->utf8",       &str2bv_fun },

    { "make-vector",    &vmake_func },
    { "vector-length",  &vlen_func  },
    { "vector-ref",     &vref_func  },
    { "vector-set!",    &vset_func  },
    { "vector->list",   &vlist_func },
    { "list->vector",   &l2v_func   },

    { "display",            &displ_fun  },
    { "open-input-file",    &fiopen_fun },
    { "open-input-string",  &siopen_fun },
    { "read-char",          &fgetc_fun  },
    { "read-u8",            &freadb_fun },
    { "read-string",        &fread_fun  },
    //{ "read-line",          &readln_fun },
    { "secd-port-info",     &pinfo_fun  },
    { "close-port",         &pclose_fun },

    { "raise",              &raise_fun },
    { "raise-continuable",  &raisec_fun },

    // misc native functions
    { "list",           &list_func  },
    { "append",         &appnd_func },
    { "eof-object?",    &eofp_func  },
    { "secd-hash",      &hash_func  },
    { "secd",           &debug_func },
    { "defined?",       &defp_func  },
    { "secd-bind!",     &bind_func  },
    { "interaction-environment", &getenv_fun },

    { NULL,       NULL        } // must be last
};

