#include "secd.h"
#include "secd_io.h"

#include "memory.h"

#define io_assert(cond, ...) \
    if (!(cond)) { \
        errorf(__VA_ARGS__); \
        return -1; \
    }

static inline bool is_port(cell_t *c) {
    return cell_type(c) == CELL_PORT;
}

static inline secd_port_ops_t * portops(secd_t *secd, cell_t *port) {
    return secd->porttypes[ port->ptype ];
}

/*
 *  Port cell management
 */
cell_t *secd_portcell_references(secd_t *secd, 
                                 cell_t **ref1, cell_t **ref2, cell_t **ref3) {
    /* TODO */
    *ref1 = *ref2 = *ref3 = SECD_NIL;
}

/*
 *  Generic port operations
 */

cell_t * secd_port_display(secd_t __unused *secd, cell_t *port) {
    io_assert(is_port(port), "secd_portdisplay: not a port\n");

    secd_portdisplay_fun_t portdisplay = portops(secd, port)->portdisplay;
    if (portdisplay)
        return portdisplay(secd, port);
    return SECD_NIL;
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
    assert(not_nil(args), "secd_port_init: no args");

    cell_t *ptypec = list_head(args);
    cell_t *pargs  = list_next(secd, args);
    assert(cell_type(ptypec) == CELL_STR, "secd_port_init: not a string");

    for (i = 0; i < SECD_MAXPORT_TYPES; ++i) {
        if (secd->porttypes[i] == NULL)
            continue;

        const char *pty = portops(secd, i)->porttype();
        if (!strcmp( pty, strmem(ptypec) )) {
            port->as.port.ptype = i;
            port->as.port.input = 0;
            port->as.port.output = 0;

            return secd->porttypes[i]->port_init(secd, port, mode, pargs);
        }
    }

    return SECD_NIL;
}

cell_t * secd_port_open(secd_t *secd, cell_t *port) {
    assert( is_port(port), "secd_port_open: not a port" );
}

/*
 * Port-reading
 */
int secd_getc(secd_t __unused *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_getc: not a port\n");
    io_assert(is_input(port), "secd_getc: not an input port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    secd_portgetc_fun_t pgetc = portops(secd, port)->portgetc;
}

/* TODO: rename */
size_t secd_fread(secd_t __unused *secd, cell_t *port, char *s, int size) {
    io_assert(cell_type(port) == CELL_PORT, "secd_fread: not a port\n");
    io_assert(is_input(port), "secd_fread: not an input port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    if (port->as.port.file) {
        FILE *f = port->as.port.as.file;
        return fread(s, size, 1, f);;
    } else {
        cell_t *str = port->as.port.as.str;
        size_t srcsize = mem_size(str);
        if (srcsize < (size_t)size) size = srcsize;

        memcpy(s, strmem(str), size);
        return size;
    }
}


int secd_port_close(secd_t *secd, cell_t *port) {
    io_assert(is_port(port), "secd_pclose: not a port\n");
    io_assert(!is_closed(port), "secd_pclose: already closed\n");

    secd_port_ops_t * ops = portops(port);

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
    iomem_t *io = (iomem_t *)port->data;
    return str->as.str.size;
}

static cell_t *iomem_display(secd_t __unused *secd, cell_t *port) {
    return SECD_NIL; /* TODO */
}

static int iomem_open(secd_t *secd, cell_t *port, const char *mode, ) {

}

static int iomem_close(secd_t __unused *secd, cell_t *port) {
    iomem_t *io = (iomem_t *)port->data;

    drop_cell(secd, io->str);
    io->str = SECD_NIL;
    return 0;
}

static int iomem_getc(secd_t *secd, cell_t *port) {
    iomem_t *io = (iomem_t *)port->data;

    cell_t *str = io->str;

    size_t size = mem_size(str);
    if (str->as.str.offset >= (int)size)
        return SECD_EOF;

    char c = strmem(str)[str->as.str.offset];
    ++str->as.str.offset;
    return (int)c;
}

const secd_port_ops_t iomem_port_ops = {
    .porttype       = iomem_porttype,

    .portinit       = iomem_init,
    .portdisplay    = iomem_display,
    .portopen       = iomem_open,
    .portsize       = iomem_size,
    .portgetc       = iomem_getc,
    .portclose      = iomem_close,
};
