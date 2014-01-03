#include "secd.h"

#include "secdops.h"
#include "readparse.h"
#include "memory.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

void print_opcode(opindex_t op) {
    if (0 <= op && op < SECD_LAST) {
        printf("#%s# ", symname(opcode_table[op].sym));
        return;
    }
    printf("#[%d]# ", op);
}

void sexp_print_atom(const cell_t *c) {
    switch (atom_type(c)) {
      case ATOM_INT: printf("%d", c->as.atom.as.num); break;
      case ATOM_SYM: printf("%s", c->as.atom.as.sym.data); break;
      case ATOM_OP: print_opcode(c->as.atom.as.op); break;
      case ATOM_FUNC: printf("(0x%p)()", c->as.atom.as.ptr); break;
      case NOT_AN_ATOM: printf("???");
    }
}

void print_cell(const cell_t *c) {
    assertv(c, "print_cell(NULL)\n");
    if (is_nil(c)) {
         printf("NIL\n");
         return;
    }
    printf("[%ld]^%ld: ", cell_index(c), c->nref);
    switch (cell_type(c)) {
      case CELL_CONS:
        printf("CONS([%ld], [%ld])\n",
               cell_index(get_car(c)), cell_index(get_cdr(c)));
        break;
      case CELL_FRAME:
        printf("FRAME(syms: [%ld], vals: [%ld])\n",
               cell_index(get_car(c)), cell_index(get_cdr(c)));
        break;
      case CELL_ATOM:
        sexp_print_atom(c); printf("\n");
        break;
      default:
        printf("unknown type: %d\n", cell_type(c));
    }
}

void print_list(cell_t *list) {
    printf("  -= ");
    while (not_nil(list)) {
        assertv(is_cons(list),
                "Not a cons at [%ld]\n", cell_index(list));
        printf("[%ld]:%ld\t", cell_index(list), cell_index(get_car(list)));
        print_cell(get_car(list));
        printf("  -> ");
        list = list_next(list);
    }
    printf("NIL\n");
}

void printc(cell_t *c) {
    assertv(c, "printc(NULL)");
    if (is_cons(c))
        print_list(c);
    else
        print_cell(c);
}

void sexp_print(cell_t *cell) {
    switch (cell_type(cell)) {
      case CELL_ATOM:
        sexp_print_atom(cell);
        break;
      case CELL_FRAME:
        printf("#<envframe> ");
        break;
      case CELL_CONS:
        printf("(");
        cell_t *iter = cell;
        while (not_nil(iter)) {
            if (iter != cell) printf(" ");
            if (cell_type(iter) != CELL_CONS) {
                printf(". "); sexp_print(iter); break;
            }

            cell_t *head = get_car(iter);
            sexp_print(head);
            iter = list_next(iter);
        }
        printf(") ");
        break;
      case CELL_ERROR:
        printf("???"); break;
      default:
        errorf("sexp_print: unknown cell type %d", (int)cell_type(cell));
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
    TOK_STR = -2,
    TOK_NUM = -3,

    TOK_QUOTE = -4,
    TOK_QQ = -5,
    TOK_UQ = -6,
    TOK_UQSPL = -7,

    TOK_ERR = -65536
};

const char not_symbol_chars[] = " ();\n";

struct secd_parser {
    token_t token;
    FILE *f;

    /* lexer guts */
    int lc;
    int numtok;
    char strtok[MAX_LEXEME_SIZE];
    char issymbc[UCHAR_MAX + 1];

    int nested;
};

cell_t *sexp_read(secd_t *secd, secd_parser_t *p);

secd_parser_t *init_parser(secd_parser_t *p, FILE *f) {
    p->lc = ' ';
    p->f = (f ? f : stdin);
    p->nested = 0;

    memset(p->issymbc, false, 0x20);
    memset(p->issymbc + 0x20, true, UCHAR_MAX - 0x20);
    const char *s = not_symbol_chars;
    while (*s)
        p->issymbc[(unsigned char)*s++] = false;
    return p;
}

secd_parser_t *new_parser(FILE *f) {
    secd_parser_t *p = (secd_parser_t *)calloc(1, sizeof(secd_parser_t));
    return init_parser(p, f);
}

inline static int nextchar(secd_parser_t *p) {
    //printf("nextchar\n");
    return p->lc = fgetc(p->f);
}

token_t lexnext(secd_parser_t *p) {
    /* skip spaces */
    while (isspace(p->lc))
        nextchar(p);

    switch (p->lc) {
      case EOF:
        return (p->token = TOK_EOF);
      case ';':
        /* comment */
        do nextchar(p); while (p->lc != '\n');
        return lexnext(p);

      case '(': case ')':
        /* one-char tokens */
        p->token = p->lc;
        nextchar(p);
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
    }

    if (isdigit(p->lc)) {
        char *s = p->strtok;
        do {
            *s++ = p->lc; nextchar(p);
        } while (isdigit(p->lc));
        *s = '\0';

        p->numtok = atoi(p->strtok);
        return (p->token = TOK_NUM);
    }

    if (p->issymbc[(unsigned char)p->lc]) {
        char *s = p->strtok;
        do {
            *s++ = p->lc;
            nextchar(p);
        } while (p->issymbc[(unsigned char)p->lc]);
        *s = '\0';

        return (p->token = TOK_STR);
    }
    return TOK_ERR;
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

cell_t *read_list(secd_t *secd, secd_parser_t *p) {
    cell_t *head = secd->nil, *tail = secd->nil;
    cell_t *newtail, *val;
    while (true) {
        int tok = lexnext(p);
        switch (tok) {
          case TOK_STR:
              val = new_symbol(secd, p->strtok);
              break;
          case TOK_NUM:
              val = new_number(secd, p->numtok);
              break;
          case TOK_EOF: case ')':
              -- p->nested;
              return head;
          case '(':
              ++ p->nested;
              val = read_list(secd, p);
              if (p->token == TOK_ERR) {
                  free_cell(head);
                  errorf("read_list: TOK_ERR\n");
                  return NULL;
              }
              if (p->token == TOK_EOF) {
                  free_cell(head);
                  errorf("read_list: TOK_EOF, ')' expected\n");
              }
              assert(p->token == ')', "read_list: not a closing bracket");
              break;

           case TOK_QUOTE: case TOK_QQ:
           case TOK_UQ: case TOK_UQSPL: {
              const char *formname = special_form_for(tok);
              assert(formname, "No formname for token=%d\n", tok);
              val = sexp_read(secd, p);
              val = new_cons(secd, new_symbol(secd, formname),
                                   new_cons(secd, val, secd->nil));
              } break;

           default:
              errorf("Unknown token: %1$d ('%1$c')", tok);
              free_cell(head);
              return NULL;
        }

        newtail = new_cons(secd, val, secd->nil);
        if (not_nil(head)) {
            tail->as.cons.cdr = share_cell(newtail);
            tail = newtail;
        } else {
            head = tail = newtail;
        }
    }
}

cell_t *sexp_read(secd_t *secd, secd_parser_t *p) {
    cell_t *inp = secd->nil;
    int tok;
    switch (tok = lexnext(p)) {
      case '(':
        inp = read_list(secd, p);
        if (p->token != ')') {
            errorf("read_secd: failed\n");
            if (inp) drop_cell(inp);
            return NULL;
        }
        break;
      case TOK_NUM:
        inp = new_number(secd, p->numtok);
        break;
      case TOK_STR:
        inp = new_symbol(secd, p->strtok);
        break;
      case TOK_EOF:
        return new_symbol(secd, EOF_OBJ);

      case TOK_QUOTE: case TOK_QQ:
      case TOK_UQ: case TOK_UQSPL: {
        const char *formname = special_form_for(tok);
        assert(formname, "No  special form for token=%d\n", tok);
        inp = sexp_read(secd, p);
        inp = new_cons(secd, new_symbol(secd, formname),
                             new_cons(secd, inp, secd->nil));
        } break;

      default:
        errorf("Unknown token: %1$d ('%1$c')", tok);
        return NULL;
    }
    return inp;
}

cell_t *sexp_parse(secd_t *secd, FILE *f) {
    secd_parser_t p;
    init_parser(&p, f);
    return sexp_read(secd, &p);
}

cell_t *read_secd(secd_t *secd, FILE *f) {
    secd_parser_t p;
    init_parser(&p, f);

    if (lexnext(&p) != '(') {
        errorf("read_secd: a list of commands expected\n");
        return NULL;
    }

    cell_t *result = read_list(secd, &p);
    if (p.token != ')') {
        errorf("read_secd: the end bracket expected\n");
        if (result) drop_cell(result);
        return NULL;
    }
    return result;
}

