#include "secd.h"

#include "secdops.h"
#include "secd_io.h"
#include "memory.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

void print_opcode(opindex_t op) {
    if (op < SECD_LAST) {
        printf("#%s# ", opcode_table[op].name);
        return;
    }
    printf("#[%d]# ", op);
}

void dbg_print_cell(secd_t *secd, const cell_t *c) {
    if (is_nil(c)) {
         printf("NIL\n");
         return;
    }
    char buf[128];
    if (c->nref > DONT_FREE_THIS - 100000) strncpy(buf, "-", 64);
    else snprintf(buf, 128, "%ld", (long)c->nref);
    printf("[%ld]^%s: ", cell_index(secd, c), buf);

    switch (cell_type(c)) {
      case CELL_CONS:
        printf("CONS([%ld], [%ld])\n",
               cell_index(secd, get_car(c)), cell_index(secd, get_cdr(c)));
        break;
      case CELL_FRAME:
        printf("FRAME(syms: [%ld], vals: [%ld])\n",
               cell_index(secd, get_car(c)), cell_index(secd, get_cdr(c)));
        break;
      case CELL_INT:  printf("%d", c->as.num); break;
      case CELL_CHAR:
        if (isprint(c->as.num)) printf("#\\%c\n", (char)c->as.num);
        else printf("#x%x\n", c->as.num);
        break;
      case CELL_OP:   print_opcode(c->as.op); break;
      case CELL_FUNC: printf("*%p()\n", c->as.ptr); break;
      case CELL_ARRAY: printf("ARR[%ld]\n",
                               cell_index(secd, arr_val(c, 0))); break;
      case CELL_STR: printf("STR[%ld\n",
                             cell_index(secd, (cell_t*)strval(c))); break;
      case CELL_SYM: printf("SYM[%08x]='%s'\n", symhash(c), symname(c)); break;
      case CELL_BYTES: printf("BVECT[%ld]\n",
                               cell_index(secd, (cell_t*)strval(c))); break;
      case CELL_REF: printf("REF[%ld]\n", cell_index(secd, c->as.ref)); break;
      case CELL_ERROR: printf("ERR[%s]\n", errmsg(c)); break;
      case CELL_ARRMETA: printf("META[%ld, %ld]\n",
                                 cell_index(secd, mcons_prev((cell_t*)c)),
                                 cell_index(secd, mcons_next((cell_t*)c))); break;
      case CELL_UNDEF: printf("#?\n"); break;
      case CELL_FREE: printf("FREE\n"); break;
      default: printf("unknown type: %d\n", cell_type(c));
    }
}

void dbg_print_list(secd_t *secd, cell_t *list) {
    printf("  -= ");
    while (not_nil(list)) {
        assertv(is_cons(list),
                "Not a cons at [%ld]\n", cell_index(secd, list));
        printf("[%ld]:%ld\t",
                cell_index(secd, list),
                cell_index(secd, get_car(list)));
        dbg_print_cell(secd, get_car(list));
        printf("  -> ");
        list = list_next(secd, list);
    }
    printf("NIL\n");
}

void dbg_printc(secd_t *secd, cell_t *c) {
    if (is_cons(c))
        dbg_print_list(secd, c);
    else
        dbg_print_cell(secd, c);
}

void sexp_print_array(secd_t *secd, const cell_t *cell) {
    const cell_t *arr = arr_val(cell, 0);
    const size_t len = arr_size(secd, cell);
    size_t i;

    printf("#(");
    for (i = cell->as.arr.offset; i < len; ++i) {
        sexp_print(secd, arr + i);
        printf(" ");
    }
    printf(")");
}

void sexp_print_bytes(secd_t __unused *secd, const cell_t *cell) {
    const unsigned char *arr = (const unsigned char *)strval(cell);
    const size_t len = mem_size(cell);
    size_t i;

    printf("#u8(");
    for (i = 0; i < len; ++i) {
        printf("#x%02x ", (int)arr[i]);
    }
    printf(")");
}

static void sexp_print_list(secd_t *secd, const cell_t *cell) {
    printf("(");
    const cell_t *iter = cell;
    while (not_nil(iter)) {
        if (iter != cell) printf(" ");
        if (cell_type(iter) != CELL_CONS) {
            printf(". "); sexp_print(secd, iter); break;
        }

        cell_t *head = get_car(iter);
        sexp_print(secd, head);
        iter = list_next(secd, iter);
    }
    printf(") ");
}

/* machine printing, (write) */
void sexp_print(secd_t* secd, const cell_t *cell) {
    switch (cell_type(cell)) {
      case CELL_UNDEF:  printf("#?"); break;
      case CELL_INT:    printf("%d", cell->as.num); break;
      case CELL_CHAR:
        if (isprint(cell->as.num)) printf("#\\%c", (char)cell->as.num);
        else printf("#\\x%x", numval(cell));
        break;
      case CELL_OP:     print_opcode(cell->as.op); break;
      case CELL_FUNC:   printf("*%p()", cell->as.ptr); break;
      case CELL_FRAME:  printf("#<envframe> "); break;
      case CELL_CONS:   sexp_print_list(secd, cell); break; break;
      case CELL_ARRAY:  sexp_print_array(secd, cell); break;
      case CELL_STR:    printf("\"%s\"", strval(cell) + cell->as.str.offset); break;
      case CELL_SYM:    printf("%s", symname(cell)); break;
      case CELL_BYTES:  sexp_print_bytes(secd, cell); break;
      case CELL_ERROR:  printf("#!\"%s\"", errmsg(cell)); break;
      case CELL_PORT:   sexp_print_port(secd, cell); break;
      default: errorf("sexp_print: unknown cell type %d", (int)cell_type(cell));
    }
}

/* human-readable, (display) */
void sexp_display(secd_t *secd, cell_t *port, cell_t *cell) {
    switch (cell_type(cell)) {
      case CELL_STR:
        secd_printf(secd, port, "%s", strval(cell));
        break;
      default: sexp_print(secd, cell);
    }
}


/*
 *  SECD parser
 *  A parser of a simple Lisp subset
 */
#define MAX_LEXEME_SIZE     256

typedef  int  token_t;
typedef  struct secd_parser secd_parser_t;

enum {
    TOK_EOF = -1,
    TOK_SYM = -2,
    TOK_NUM = -3,
    TOK_STR = -4,
    TOK_CHAR = -5,

    TOK_QUOTE = -6,
    TOK_QQ = -7,
    TOK_UQ = -8,
    TOK_UQSPL = -9,

    TOK_ERR = -65536
};

const char not_symbol_chars[] = " ();\n\t";

struct secd_parser {
    secd_t *secd;
    token_t token;

    /* lexer guts */
    int lc;     // lex char
    int numtok;
    char symtok[MAX_LEXEME_SIZE];
    char issymbc[UCHAR_MAX + 1];

    char *strtok;

    int nested;
};

cell_t *sexp_read(secd_t *secd, secd_parser_t *p);
cell_t *read_list(secd_t *secd, secd_parser_t *p);


secd_parser_t *init_parser(secd_t *secd, secd_parser_t *p) {
    p->lc = ' ';
    p->nested = 0;
    p->secd = secd;

    memset(p->issymbc, false, 0x20);
    memset(p->issymbc + 0x20, true, UCHAR_MAX - 0x20);
    const char *s = not_symbol_chars;
    while (*s)
        p->issymbc[(unsigned char)*s++] = false;
    return p;
}

inline static int nextchar(secd_parser_t *p) {
    secd_t *secd = p->secd;
    return p->lc = secd_getc(secd, secd->input_port);
}

inline static bool isbasedigit(int c, int base) {
    switch (base) {
      case 2: return (base == '0') || (base == '1');
      case 8: return (base <= '0') && (base <= '7');
      case 10: return isdigit(c);
      case 16: return isxdigit(c);
    }
    return false;
}

inline static token_t lexnumber(secd_parser_t *p, int base) {
    char *s = p->symtok;
    do {
        *s++ = p->lc;
        nextchar(p);
    } while (isbasedigit(p->lc, base));
    *s = '\0';

    char *end = NULL;
    p->numtok = (int)strtol(p->symtok, &end, base);
    if (end[0] != '\0')
        return (p->token = TOK_ERR);
    return (p->token = TOK_NUM);
}

inline static token_t lexsymbol(secd_parser_t *p) {
    char *s = p->symtok;
    size_t read_count = 1;
    do {
        *s++ = p->lc;
        nextchar(p);
        if (++read_count >= MAX_LEXEME_SIZE) {
            *s = '\0';
            errorf("lexnext: lexeme is too large: %s\n", p->symtok);
            return (p->token = TOK_ERR);
        }
    } while (p->issymbc[(unsigned char)p->lc]);
    *s = '\0';

    /* try to convert symbol into number */
    if (p->symtok[0] == '-' || p->symtok[0] == '+') {
        char *end = NULL;
        p->numtok = (int)strtol(p->symtok, &end, 10);
        if ((p->symtok[0] != '\0') && (end[0] == '\0'))
            return (p->token = TOK_NUM);
    }

    return (p->token = TOK_SYM);
}

inline static token_t lexstring(secd_parser_t *p) {
    size_t bufsize = 256;      /* initial size since string size is not limited */
    size_t read_count = 0;
    char *buf = malloc(bufsize); /* to be freed after p->strtok is consumed */
    while (1) {
        nextchar(p);
        switch (p->lc) {
          case '\\':
            nextchar(p);
            switch (p->lc) {
              case 'a' : buf[read_count++] = '\x07'; break;
              case 'b' : buf[read_count++] = '\x08'; break;
              case 't' : buf[read_count++] = '\x09'; break;
              case 'n' : buf[read_count++] = '\x0A'; break;
              case 'x': {
                    char hexbuf[10];
                    char *hxb = hexbuf;

                    nextchar(p);
                    if (!isxdigit(p->lc))
                        goto cleanup_and_exit;
                    do {
                        *hxb++ = p->lc;
                        nextchar(p);
                    } while ((hxb - hexbuf < 9) && isxdigit(p->lc));
                    if (p->lc != ';')
                        goto cleanup_and_exit;

                    *hxb = '\0';
                    unichar_t charcode = (int)strtol(hexbuf, NULL, 16);
                    char *after = utf8cpy(buf + read_count, charcode);
                    if (!after)
                        goto cleanup_and_exit;

                    read_count = after - buf;
                } break;
              default:
                buf[read_count++] = p->lc;
            }
            break;
          case '"':
            nextchar(p);
            buf[read_count] = '\0';
            p->strtok = buf;    /* don't forget to free */
            return (p->token = TOK_STR);
          default:
            buf[read_count] = p->lc;
            ++read_count;
            if (read_count + 4 >= bufsize) { // +4 because of utf8cpy
                /* reallocate */
                bufsize *= 2;
                buf = realloc(buf, bufsize);
                if (!buf) {
                    errorf("lexstring: not enough memory for a string\n");
                    return TOK_ERR;
                }
            }
        }
    }
cleanup_and_exit:
    free(buf);
    return (p->token = TOK_ERR);
}

const struct {
    const char *name;
    int chr;
} scheme_chars_names[] = {
    { "alarm",     '\x07' },
    { "backspace", '\x08' },
    { "delete",    '\x7f' },
    { "escape",    '\x1b' },
    { "newline",   '\x0a' },
    { "null",      '\x00' },
    { "return",    '\x0d' },
    { "space",     ' ' },
    { "tab",       '\t' },

    { NULL,        0 }
};

token_t lexchar(secd_parser_t *p) {
    char *s = p->symtok;
    while (p->issymbc[p->lc]
           && ((s - p->symtok) < MAX_LEXEME_SIZE))
    {
        *s++ = p->lc;
        nextchar(p);
    }
    *s = '\0';

    if (p->symtok[0] == '\0') {
        p->numtok = p->lc;
        nextchar(p);
        return (p->token = TOK_CHAR);
    }
    if (p->symtok[1] == '\0') {
        p->numtok = p->symtok[0];
        return (p->token = TOK_CHAR);
    }
    if (p->symtok[0] == 'x') {
        char *end = NULL;
        p->numtok = (int)strtol(p->symtok + 1, &end, 16);
        if (end && (end[0] == '\0'))
            return (p->token = TOK_CHAR);
    }
    int i = 0;
    for (i = 0; scheme_chars_names[i].name; ++i) {
        if (scheme_chars_names[i].name[0] > p->symtok[0])
            break;
        if (str_eq(scheme_chars_names[i].name, p->symtok)) {
            p->numtok = scheme_chars_names[i].chr;
            return (p->token = TOK_CHAR);
        }
    }

    return (p->token = TOK_ERR);
}

token_t lexnext(secd_parser_t *p) {
    /* skip spaces */
    while (isspace(p->lc))
        nextchar(p);

    switch (p->lc) {
      case EOF: return (p->token = TOK_EOF);
      case ';':
        /* consume comment */
        do nextchar(p); while (p->lc != '\n');
        return lexnext(p);

      case '(': case ')':
        p->token = p->lc;
        nextchar(p);
        return p->token;

      case '#':
        /* one-char tokens */
        p->token = p->lc;
        nextchar(p);
        switch (p->lc) {
            case 'f': case 't':
                p->symtok[0] = '#';
                p->symtok[1] = p->lc;
                p->symtok[2] = '\0';
                nextchar(p);
                return (p->token = TOK_SYM);
            /* chars */
            case '\\': nextchar(p); return lexchar(p);
            /* numbers */
            case 'x': case 'X': nextchar(p); return lexnumber(p, 16);
            case 'o': case 'O': nextchar(p); return lexnumber(p, 8);
            case 'b': case 'B': nextchar(p); return lexnumber(p, 2);
            case 'd': case 'D': nextchar(p); return lexnumber(p, 10);
        }
        return p->token;
      case '\'':
        nextchar(p);
        return (p->token = TOK_QUOTE);
      case '`':
        nextchar(p);
        return (p->token = TOK_QQ);
      case ',':
        /* may be ',' or ',@' */
        nextchar(p);
        if (p->lc == '@') {
            nextchar(p);
            return (p->token = TOK_UQSPL);
        }
        return (p->token = TOK_UQ);
      case '"':
        return lexstring(p);
    }

    if (isdigit(p->lc))
        return lexnumber(p, 10);

    if (p->issymbc[(unsigned char)p->lc])
        return lexsymbol(p);

    return TOK_ERR; /* nothing fits */
}

static const char * special_form_for(int token) {
    switch (token) {
      case TOK_QUOTE: return "quote";
      case TOK_QQ:    return "quasiquote";
      case TOK_UQ:    return "unquote";
      case TOK_UQSPL: return "unquote-splicing";
    }
    return NULL;
}

static cell_t *read_bytevector(secd_parser_t *p) {
    secd_t *secd = p->secd;
    assert(p->token == '(', "read_bytevector: '(' expected");
    cell_t *tmplist = SECD_NIL;
    cell_t *cur;
    size_t len = 0;
    while (lexnext(p) == TOK_NUM) {
        assert((0 <= p->numtok) && (p->numtok < 256), "read_bytevector: out of range");

        cell_t *newc = new_cons(secd, new_number(secd, p->numtok), SECD_NIL);
        if (not_nil(tmplist)) {
            cur->as.cons.cdr = share_cell(secd, newc);
            cur = newc;
        } else {
            tmplist = cur = newc;
        }
        ++len;
    }

    cell_t *bvect = new_bytevector_of_size(secd, len);
    assert_cell(bvect, "read_bytevector: failed to allocate");
    unsigned char *mem = (unsigned char *)strmem(bvect);

    cur = tmplist;
    size_t i;
    for (i = 0; i < len; ++i) {
        mem[i] = (unsigned char)numval(list_head(cur));
        cur = list_next(secd, cur);
    }

    free_cell(secd, tmplist);
    return bvect;
}

static cell_t *read_token(secd_t *secd, secd_parser_t *p) {
    int tok;
    cell_t *inp = &secd_nil_failure;
    switch (tok = p->token) {
      case '(':
        ++p->nested;
        inp = read_list(secd, p);
        if (p->token != ')')
            goto error_exit;
        return inp;
      case TOK_NUM:
        return new_number(secd, p->numtok);
      case TOK_CHAR:
        return new_char(secd, p->numtok);
      case TOK_SYM:
        return new_symbol(secd, p->symtok);
      case TOK_STR:
        inp = new_string(secd, p->strtok);
        free(p->strtok);
        return inp;
      case TOK_EOF:
        return new_symbol(secd, EOF_OBJ);

      case TOK_QUOTE: case TOK_QQ:
      case TOK_UQ: case TOK_UQSPL: {
        const char *formname = special_form_for(tok);
        assert(formname, "No  special form for token=%d\n", tok);
        inp = sexp_read(secd, p);
        assert_cell(inp, "sexp_read: reading subexpression failed");
        return new_cons(secd, new_symbol(secd, formname), new_cons(secd, inp, SECD_NIL));
      }

      case '#':
        switch (tok = lexnext(p)) {
          case '(': {
              cell_t *tmplist = read_list(secd, p);
              if (p->token != ')') {
                  free_cell(secd, tmplist);
                  goto error_exit;
              }
              inp = list_to_vector(secd, tmplist);
              free_cell(secd, tmplist);
              return inp;
            }
          case TOK_SYM: {
              if (str_eq(p->symtok, "u8")) {
                  lexnext(p);
                  inp = read_bytevector(p);
                  if (p->token != ')')
                      goto error_exit;
                  return inp;
              }
          }
        }
        errorf("Unknown suffix for #\n");
    }

error_exit:
    if (inp) free_cell(secd, inp);
    errorf("read_token: failed\n");
    return new_error(secd, "read_token: failed on token %1$d '%1$c'", p->token);
}

cell_t *read_list(secd_t *secd, secd_parser_t *p) {
    const char *parse_err = NULL;
    cell_t *head = SECD_NIL;
    cell_t *tail = SECD_NIL;

    cell_t *newtail, *val;

    while (true) {
        int tok = lexnext(p);
        switch (tok) {
          case TOK_EOF: case ')':
              -- p->nested;
              return head;

          case '(':
              ++ p->nested;
              val = read_list(secd, p);
              if (p->token == TOK_ERR) {
                  parse_err = "read_list: error reading subexpression";
                  goto error_exit;
              }
              if (p->token != ')') {
                  parse_err = "read_list: TOK_EOF, ')' expected";
                  goto error_exit;
              }
              break;

           default:
              val = read_token(secd, p);
              if (is_error(val)) {
                  parse_err = "read_list: read_token failed";
                  goto error_exit;
              }
              /* reading dot-lists */
              if (is_symbol(val) && (str_eq(symname(val), "."))) {
                  free_cell(secd, val);

                  switch (lexnext(p)) {
                    case TOK_ERR: case ')':
                      parse_err = "read_list: failed to read a token after dot";
                      goto error_exit;
                    case '(':
                      /* there may be a list after dot */
                      val = read_list(secd, p);
                      if (p->token != ')') {
                          parse_err = "read_list: expected a ')' reading sublist after dot";
                          goto error_exit;
                      }
                      lexnext(p); // consume ')'
                      break;

                    default:
                      val = read_token(secd, p);
                      lexnext(p);
                  }

                  if (is_nil(head)) /* Guile-like: (. val) returns val */
                      return val;
                  tail->as.cons.cdr = share_cell(secd, val);
                  return head;
              }
        }

        newtail = new_cons(secd, val, SECD_NIL);
        if (not_nil(head)) {
            tail->as.cons.cdr = share_cell(secd, newtail);
            tail = newtail;
        } else {
            head = tail = newtail;
        }
    }
error_exit:
    free_cell(secd, head);
    errorf("read_list: TOK_ERR, %s\n", parse_err);
    return new_error(secd, parse_err);
}

cell_t *sexp_read(secd_t *secd, secd_parser_t *p) {
    lexnext(p);
    return read_token(secd, p);
}

cell_t *sexp_parse(secd_t *secd, cell_t *port) {
    cell_t *prevport = SECD_NIL;
    if (not_nil(port)) {
        assert(cell_type(port) == CELL_PORT, "sexp_parse: not a port");
        prevport = secd->input_port; // share_cell, drop_cell
        secd->input_port = share_cell(secd, port);
    }

    secd_parser_t p;
    init_parser(secd, &p);
    cell_t *res = sexp_read(secd, &p);

    if (not_nil(prevport)) {
        secd->input_port = prevport; //share_cell back
        drop_cell(secd, port);
    }
    return res;
}

