#ifndef __SECD_READPARSE_H__
#define __SECD_READPARSE_H__

void print_cell(secd_t *secd, const cell_t *c);
void printc(secd_t *secd, cell_t *c);

void sexp_print(secd_t *secd, cell_t *c);

cell_t *sexp_parse(secd_t *secd, FILE *f);
cell_t *read_secd(secd_t *secd, FILE *f);

#endif //__SECD_READPARSE_H__
