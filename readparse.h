#ifndef __SECD_READPARSE_H__
#define __SECD_READPARSE_H__

void print_cell(const cell_t *c);
void print_cell(const cell_t *c);
void printc(cell_t *c);

void sexp_print(cell_t *c);

cell_t *sexp_parse(secd_t *secd, FILE *f);

#endif //__SECD_READPARSE_H__
