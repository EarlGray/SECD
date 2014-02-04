#ifndef __SECD_IO_H_
#define __SECD_IO_H_

#define SECD_EOF    (-1)

cell_t *secd_stdin(secd_t *secd);
cell_t *secd_stdout(secd_t *secd);

cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode);
long secd_portsize(secd_t *secd, cell_t *port);
int secd_pclose(secd_t *secd, cell_t *port);

int secd_getc(secd_t *secd, cell_t *port);
size_t secd_fread(secd_t *secd, cell_t *port, char *s, int size);

int secd_printf(secd_t *secd, cell_t *port, const char *format, ...);
int secd_vprintf(secd_t *secd, cell_t *port, const char *format, va_list ap);

void sexp_print_port(secd_t *secd, const cell_t *port);

#endif //__SECD_IO_H_;
