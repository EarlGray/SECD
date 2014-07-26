#include "secd.h"
#include "secd_io.h"

#include "memory.h"

#include <string.h>

#define io_assert(cond, ...) \
    if (!(cond)) { \
        errorf(__VA_ARGS__); \
        return -1; \
    }

static inline bool is_port(cell_t *c) {
    return cell_type(c) == CELL_PORT;
}

static inline secd_port_ops_t * portops(secd_t *secd, cell_t *port) {
    return secd->porttypes[ port->as.port.ptype ];
}

/*
 *  Port cell management
 */
cell_t *secd_portcell_references(secd_t *secd, cell_t *port,
                                 cell_t **ref1, cell_t **ref2, cell_t **ref3) {
    /* TODO */
    *ref1 = *ref2 = *ref3 = SECD_NIL;
    secd_portcellrefs_fun_t pcell_refs = portops(secd, port)->pcell_refs;
    if (!pcell_refs)
        return port;
    pcell_refs(secd, port, ref1, ref2, ref3);
    return port;
}

/*
 *  Generic port operations
 */

/* for port cell serialization */
cell_t * secd_port_info(secd_t __unused *secd, cell_t *port) {
    assert(is_port(port), "secd_portdisplay: not a port\n");

    secd_portinfo_fun_t pinfo = portops(secd, port)->portdisplay;
    if (!pinfo)
        return SECD_NIL;

    return pinfo(secd, port);
}

/* for pretty-printing */
int sexp_snprint_port(secd_t *secd, char *buf, size_t buflen, const cell_t *port) {
    if (is_closed(port)) {
        snprintf(buf, buflen, "#port:closed");
        return;
    }
    bool in = port->as.port.input;
    bool out = port->as.port.output;

    secd_portinfo_fun_t pinfo;
    return snprintf(buf, buflen, "#port:%s@%ld",
                  portops(secd, port)->porttype(),
                  cell_index(secd, port));
}

long secd_port_size(secd_t __unused *secd, cell_t *port) {
    io_assert(is_port(port), "secd_portsize: not a port\n");

    secd_portsize_fun_t portsize = portops(secd, port)->portsize;

    if (portsize)
        return portsize(secd, port);
    return -1;
}

cell_t *secd_port_init(secd_t *secd,
                       cell_t *port,
                       const char *mode,
                       cell_t *args)
{
    int i;
    assert(not_nil(args), "secd_port_init: no args");

    cell_t *ptypec = list_head(args);
    cell_t *pargs  = list_next(secd, args);
    assert(cell_type(ptypec) == CELL_STR, "secd_port_init: not a string");

    for (i = 0; i < SECD_MAXPORT_TYPES; ++i) {
        if (secd->porttypes[i] == NULL)
            continue;

        const char *pty = secd->porttypes[i]->porttype();
        if (!strcmp( pty, strmem(ptypec) )) {
            port->as.port.ptype = i;
            port->as.port.input = 0;
            port->as.port.output = 0;

            secd_portinit_fun_t portinit = secd->porttypes[i]->portinit;
            if (!portinit) {
                errorf(";; secd_port_init: no portinit() method for %s\n", pty);
                return SECD_NIL;
            }
            return portinit(secd, port, mode, pargs);
        }
    }

    return SECD_NIL;
}

cell_t * secd_port_open(secd_t __unused *secd, cell_t *port) {
    assert( is_port(port), "secd_port_open: not a port" );

    secd_portopen_fun_t popen = portops(secd, port)->portopen;
    if (!popen)
        return SECD_NIL;
    return popen(secd, port);
}

/*
 * Port-reading
 */
int secd_getc(secd_t __unused *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_getc: not a port\n");
    io_assert(is_input(port), "secd_getc: not an input port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    secd_portgetc_fun_t pgetc = portops(secd, port)->portgetc;
    return pgetc(secd, port);
}

/* TODO: rename */
size_t secd_port_read(secd_t *secd, cell_t *port, char *s, int size) {
    io_assert(cell_type(port) == CELL_PORT, "secd_fread: not a port\n");
    io_assert(is_input(port), "secd_fread: not an input port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    secd_portread_fun_t pread = portops(secd, port)->portread;
    return pread(secd, port, s, size);
}


int secd_port_close(secd_t *secd, cell_t *port) {
    io_assert(is_port(port), "secd_pclose: not a port\n");
    io_assert(!is_closed(port), "secd_pclose: already closed\n");

    secd_port_ops_t * ops = portops(secd, port);

    int ret = 0;
    if (ops->portclose)
        ret = ops->portclose(secd, port);

    if (!ret) {
        port->as.port.input = false;
        port->as.port.output = false;
    }
    return ret;
}

/*
 *   Implementation of memory port
 */

typedef struct {
    cell_t *str;
} iomem_t;

static const char * iomem_porttype(void) {
    return "string";
}

static long iomem_size(secd_t __unused *secd, cell_t *port) {
    iomem_t *io = (iomem_t *)port->as.port.data;
    return io->str->as.str.size;
}

static cell_t *iomem_display(secd_t __unused *secd, cell_t *port) {
    /* TODO */
    return SECD_NIL;
}

static cell_t *iomem_init(secd_t *secd, cell_t *port, const char *mode, cell_t *pargs) {
    /* TODO */
    return port;
}

static int iomem_open(secd_t __unused *secd, cell_t __unused *port) {
    return 0;
}

static size_t iomem_read(secd_t __unused *secd, cell_t *port, char *buf, size_t buflen) {
    iomem_t *io = (iomem_t *)port->as.port.data;

    size_t copied = buflen;
    size_t srcsize = mem_size(io->str);
    if (srcsize < buflen) copied = srcsize;

    memcpy(buf, strmem(io->str), copied);
    return copied;
}

static int iomem_close(secd_t __unused *secd, cell_t *port) {
    iomem_t *io = (iomem_t *)port->as.port.data;

    drop_cell(secd, io->str);
    io->str = SECD_NIL;
    return 0;
}

static int iomem_getc(secd_t __unused *secd, cell_t *port) {
    iomem_t *io = (iomem_t *)port->as.port.data;

    cell_t *str = io->str;

    size_t size = mem_size(str);
    if (str->as.str.offset >= (int)size)
        return SECD_EOF;

    char c = strmem(str)[str->as.str.offset];
    ++str->as.str.offset;
    return (int)c;
}

static int iomem_cellrefs(secd_t *secd, cell_t *port,
                          cell_t **ref1, cell_t **ref2, cell_t **ref3)
{
    iomem_t *io = (iomem_t *)port->as.port.data;
    *ref1 = io->str;
    return 1;
}

const secd_port_ops_t iomem_port_ops = {
    .porttype       = iomem_porttype,
    .pcell_refs     = iomem_cellrefs,

    .portinit       = iomem_init,
    .portdisplay    = iomem_display,
    .portopen       = iomem_open,
    .portsize       = iomem_size,
    .portread       = iomem_read,
    .portgetc       = iomem_getc,
    .portclose      = iomem_close,
};
