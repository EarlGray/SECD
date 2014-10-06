#ifndef __SECD_IO_H_
#define __SECD_IO_H_

#include "secd.h"

#define SECD_EOF    (-1)
#define EOF_OBJ     "#<eof>"

enum secd_portstd {
    SECD_STDIN,
    SECD_STDOUT,
    SECD_STDERR,
    SECD_STDDBG,
};

typedef const char * (*portinfo_func_t)(secd_t *, cell_t *, cell_t **);
typedef int (*portopen_func_t)(secd_t *, cell_t *, const char *mode, cell_t *params);
typedef int (*portgetc_func_t)(secd_t *, cell_t *);
typedef long (*portsize_func_t)(secd_t *, cell_t *);
typedef int (*portvprintf_func_t)(secd_t *, cell_t *, const char *, va_list);
typedef size_t (*portread_func_t)(secd_t *, cell_t *, size_t, char *);
typedef int (*portclose_func_t)(secd_t *, cell_t *);
typedef cell_t *(*portowns_func_t)(secd_t*, cell_t *,cell_t **, cell_t **, cell_t **);
typedef cell_t *(*portstd_func_t)(secd_t*, enum secd_portstd);

struct portops {
    portinfo_func_t pinfo;
    portopen_func_t popen;
    portgetc_func_t pgetc;
    portread_func_t pread;
    portvprintf_func_t pvprintf;
    portsize_func_t psize;
    portclose_func_t pclose;
    portowns_func_t powns;
    portstd_func_t pstd;
};


cell_t *secd_stdin(secd_t *secd);
cell_t *secd_stdout(secd_t *secd);
cell_t *secd_stderr(secd_t *secd);
cell_t *secd_stddbg(secd_t *secd);
cell_t *secd_set_dbg(secd_t *secd, cell_t *dbgport);

int secd_popen(secd_t *secd, cell_t *p, const char *mode, cell_t *info);
long secd_portsize(secd_t *secd, cell_t *port);
int secd_pclose(secd_t *secd, cell_t *port);

int secd_pgetc(secd_t *secd, cell_t *port);
size_t secd_pread(secd_t *secd, cell_t *port, char *s, int size);

int secd_printf(secd_t *secd, const char *format, ...);
int secd_pprintf(secd_t *secd, cell_t *port, const char *format, ...);
int secd_vpprintf(secd_t *secd, cell_t *port, const char *format, va_list ap);

void sexp_print_port(secd_t *secd, const cell_t *port);
void sexp_pprint_port(secd_t *secd, cell_t *p, const cell_t *port);

cell_t *secd_pserialize(secd_t *secd, cell_t *p);
cell_t *secd_newport(secd_t *secd, const char *mode, const char *ty, cell_t *params);
cell_t *secd_newport_by_name(secd_t *secd, const char *mode, const char *ty, const char * name);
cell_t *secd_port_owns(secd_t *secd, cell_t *p, cell_t **, cell_t **, cell_t **);

const char * secd_porttyname(secd_t *secd, int ty);
int secd_pdump_array(secd_t *secd, cell_t *p, cell_t *mcons);

static inline bool is_closed(cell_t *port) {
    return !port->as.port.input && !port->as.port.output;
}

void secd_init_ports(secd_t *secd);

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

inline static cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode) {
    return secd_newport_by_name(secd, mode, "file", fname);
}

#endif //__SECD_IO_H_;
