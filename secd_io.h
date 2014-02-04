#ifndef __SECD_IO_H_
#define __SECD_IO_H_

cell_t *secd_stdin(secd_t *secd);
cell_t *secd_stdout(secd_t *secd);

cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode);

int secd_getc(secd_t *secd, cell_t *port);

int secd_printf(secd_t *secd, cell_t *port, const char *format, ...);
int secd_vprintf(secd_t *secd, cell_t *port, const char *format, va_list ap);

#endif //__SECD_IO_H_;
