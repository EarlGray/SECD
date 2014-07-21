#ifndef __SECD_IO_H_
#define __SECD_IO_H_

#include "secd.h"

#define SECD_EOF    (-1)

#define SECD_MAX_PORTTYPE_COUNT  8

cell_t *secd_stdin(secd_t *secd);
cell_t *secd_stdout(secd_t *secd);
cell_t *secd_stderr(secd_t *secd);
cell_t *secd_stddbg(secd_t *secd);
cell_t *secd_set_dbg(secd_t *secd, cell_t *dbgport);
cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode);

/*
 *  Generic port operations
 */
cell_t *secd_port_init(secd_t *secd, cell_t *port, const char *mode, cell_t *args);

long secd_port_size(secd_t *secd, cell_t *port);

int secd_port_close(secd_t *secd, cell_t *port);

cell_t *secd_portdisplay(secd_t *secd, cell_t *port);

cell_t *secd_portcell_references(secd_t *secd, 
                                 cell_t **ref1, cell_t **ref2, cell_t **ref3);

int secd_getc(secd_t *secd, cell_t *port);
size_t secd_fread(secd_t *secd, cell_t *port, char *s, int size);

int secd_printf(secd_t *secd, cell_t *port, const char *format, ...);
int secd_vprintf(secd_t *secd, cell_t *port, const char *format, va_list ap);

void sexp_print_port(secd_t *secd, const cell_t *port);

static inline bool is_closed(cell_t *port) {
    return !port->as.port.input && !port->as.port.output;
}


/*
 *  Port plugin system
 */
typedef cell_t * (*secd_portdisplay_fun_t)(secd_t *, cell_t*);

struct secd_port_ops {
    secd_portdisplay_fun_t      portdisplay;
    /* TODO */
};

void secd_register_porttype(secd_t *, secd_port_ops_t *ops);

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
