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

size_t secd_port_read(secd_t *secd, cell_t *port, char *s, int size);

int secd_port_close(secd_t *secd, cell_t *port);

cell_t *secd_port_info(secd_t *secd, cell_t *port);

cell_t *secd_portcell_references(secd_t *secd, cell_t *port,
                                 cell_t **ref1, cell_t **ref2, cell_t **ref3);

int secd_getc(secd_t *secd, cell_t *port);
size_t secd_fread(secd_t *secd, cell_t *port, char *s, int size);

int secd_printf(secd_t *secd, cell_t *port, const char *format, ...);
int secd_vprintf(secd_t *secd, cell_t *port, const char *format, va_list ap);

int sexp_snprint_port(secd_t *secd, char *buf, size_t buflen, const cell_t *port);

static inline bool is_closed(const cell_t *port) {
    return !port->as.port.input && !port->as.port.output;
}


/*
 *  Port plugin system
 */
typedef const char * (*secd_porttype_fun_t)(void);
typedef cell_t * (*secd_portinit_fun_t) (secd_t *, cell_t *, const char *, cell_t *);
typedef cell_t * (*secd_portdisplay_fun_t)(secd_t *, cell_t*);
typedef cell_t * (*secd_portinfo_fun_t) (secd_t *, cell_t*);
typedef long     (*secd_portsize_fun_t) (secd_t *, cell_t *);
typedef int      (*secd_portgetc_fun_t) (secd_t *, cell_t *);
typedef int      (*secd_portopen_fun_t) (secd_t *, cell_t *);
typedef size_t   (*secd_portread_fun_t) (secd_t *, cell_t *, char *, size_t);
typedef int      (*secd_portclose_fun_t) (secd_t *, cell_t *);
typedef int     (*secd_portcellrefs_fun_t) (secd_t *, cell_t *,
                                            cell_t **ref1, cell_t **ref2, cell_t **ref3);

struct secd_port_ops {
    secd_porttype_fun_t         porttype;

    secd_portcellrefs_fun_t     pcell_refs;

    secd_portdisplay_fun_t      portdisplay;
    secd_portsize_fun_t         portsize;

    secd_portinit_fun_t         portinit;
    secd_portopen_fun_t         portopen;
    secd_portread_fun_t         portread;
    secd_portclose_fun_t        portclose;

    secd_portgetc_fun_t         portgetc;
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
