#ifndef __SECD_IO_H_
#define __SECD_IO_H_

#include "secd.h"

#define SECD_EOF    (-1)
#define EOF_OBJ     "#<eof>"
#define SECD_EXC_HANDLERS  "*secd-exception-handlers*"

cell_t *secd_stdin(secd_t *secd);
cell_t *secd_stdout(secd_t *secd);
cell_t *secd_stderr(secd_t *secd);
cell_t *secd_stddbg(secd_t *secd);
cell_t *secd_set_dbg(secd_t *secd, cell_t *dbgport);

cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode);
long secd_portsize(secd_t *secd, cell_t *port);
int secd_pclose(secd_t *secd, cell_t *port);

int secd_getc(secd_t *secd, cell_t *port);
size_t secd_fread(secd_t *secd, cell_t *port, char *s, int size);

int secd_printf(secd_t *secd, const char *format, ...);
int secd_pprintf(secd_t *secd, cell_t *port, const char *format, ...);
int secd_vpprintf(secd_t *secd, cell_t *port, const char *format, va_list ap);

void sexp_print_port(secd_t *secd, const cell_t *port);
void sexp_pprint_port(secd_t *secd, cell_t *p, const cell_t *port);

int secd_pdump_array(secd_t *secd, cell_t *p, cell_t *mcons);

static inline bool is_closed(cell_t *port) {
    return !port->as.port.input && !port->as.port.output;
}

#include "conf.h"

#if (MEMDEBUG)
# define memdebugf(...) if (not_nil(secd->debug_port)) { \
    secd_printf(secd, secd->debug_port, "%ld |   ", secd->tick); \
    secd_printf(secd, secd->debug_port, __VA_ARGS__);  \
  }
# if (MEMTRACE)
#  define memtracef(...) printf(__VA_ARGS__)
# else
#  define memtracef(...)
# endif
#else
# define memdebugf(...)
# define memtracef(...)
#endif

#if (CTRLDEBUG)
# define ctrldebugf(...) if (not_nil(secd->debug_port)) { \
    secd_printf(secd, secd->debug_port, "%ld | ", secd->tick); \
    secd_printf(secd, secd->debug_port, __VA_ARGS__);  \
  }
#else
# define ctrldebugf(...)
#endif

#if (ENVDEBUG)
# define envdebugf(...) secd_printf(secd, secd->debug_port, __VA_ARGS__)
#else
# define envdebugf(...)
#endif

#ifndef __unused
# define __unused __attribute__((unused))
#endif

#endif //__SECD_IO_H_;
