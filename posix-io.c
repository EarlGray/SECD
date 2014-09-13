#include "secd.h"
#include "secd_io.h"
#include "memory.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define io_assert(cond, ...) \
    if (!(cond)) { \
        errorf(__VA_ARGS__); \
        return -1; \
    }

cell_t *secd_stdin(secd_t *secd) {
    return new_fileport(secd, stdin, "r");
}

cell_t *secd_stdout(secd_t *secd) {
    return new_fileport(secd, stdout, "w");
}

cell_t *secd_stderr(secd_t *secd) {
    return new_fileport(secd, stderr, "w");
}

cell_t *secd_stddbg(secd_t __unused *secd) {
    return SECD_NIL;
}
cell_t *secd_set_dbg(secd_t *secd, cell_t *dbgport) {
    secd->debug_port = share_cell(secd, dbgport);
    return secd->debug_port;
}

cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode) {
    FILE *f = fopen(fname, mode);
    assert(f, "secd_fopen('%s'): %s\n", fname, strerror(errno));

    cell_t *cmdport = new_fileport(secd, f, mode);
    assert_cellf(cmdport, "secd_fopen: failed to create port for '%s'\n", fname);
    return cmdport;
}

long secd_portsize(secd_t __unused *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_portsize: not a port\n");

    if (port->as.port.file) {
        FILE *f = port->as.port.as.file;
        long curpos = ftell(f);

        if (!fseek(f, 0, SEEK_END))
            return -1; /* file is not seekable */

        long endpos = ftell(f);
        fseek(f, curpos, SEEK_SET);
        return endpos;
    } else {
        cell_t *str = port->as.port.as.str;
        return mem_size(str);
    }
}

int secd_pclose(secd_t *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_pclose: not a port\n");
    io_assert(!is_closed(port), "secd_pclose: already closed\n");

    int ret = 0;
    if (port->as.port.file) {
        ret = fclose(port->as.port.as.file);
        port->as.port.as.file = NULL;
    } else {
        drop_cell(secd, port->as.port.as.str);
        port->as.port.as.str = SECD_NIL;
    }
    port->as.port.input = false;
    port->as.port.output = false;
    return ret;
}

/*
 * Port-reading
 */
int secd_getc(secd_t __unused *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_getc: not a port\n");
    io_assert(is_input(port), "secd_getc: not an input port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    if (port->as.port.file) {
        int c = fgetc(port->as.port.as.file);
        if (c != EOF)
            return c;
        return SECD_EOF;
    } else {
        cell_t *str = port->as.port.as.str;
        size_t size = mem_size(str);
        if (str->as.str.offset >= (int)size)
            return EOF;

        char c = strmem(str)[str->as.str.offset];
        if (c == '\0')
            return SECD_EOF;
        ++str->as.str.offset;
        return (int)c;
    }
}

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

/*
 * Port-printing
 */
int secd_vpprintf(secd_t __unused *secd, cell_t *port, const char *format, va_list ap) {
    io_assert(cell_type(port) == CELL_PORT, "vpprintf: not a port\n");
    io_assert(is_output(port), "vpprintf: not an output port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    int ret;

    if (port->as.port.file) {
        ret = vfprintf(port->as.port.as.file, format, ap);
    } else {
        cell_t *str = port->as.port.as.str;
        char *mem = strmem(str);
        size_t offset = str->as.str.offset;
        size_t size = mem_size(str) - offset;
        ret = vsnprintf(mem, size, format, ap);
        if (ret == (int)size) {
            errorf("vpprintf: string is too small");
            errorf("vpprintf: TODO: resize string");
            ret = -1;
        }
    }
    return ret;
}

int secd_printf(secd_t *secd, const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    int ret = secd_vpprintf(secd, secd->output_port, format, ap);
    va_end(ap);

    return ret;
}

int secd_pprintf(secd_t *secd, cell_t *port, const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    int ret = secd_vpprintf(secd, port, format, ap);
    va_end(ap);

    return ret;
}

void sexp_pprint_port(secd_t *secd, cell_t *p, const cell_t *port) {
    if (SECD_NIL == port->as.port.as.str) {
        secd_pprintf(secd, p, "#<closed>");
        return;
    }
    bool in = port->as.port.input;
    bool out = port->as.port.output;
    secd_pprintf(secd, p, "#<%s%s%s: ", (in ? "input" : ""), 
            (out ? "output" : ""), (in && out ? "/" : ""));

    if (port->as.port.file) {
        secd_pprintf(secd, p, "file %d", fileno(port->as.port.as.file));
    } else {
        secd_pprintf(secd, p, "string %ld", cell_index(secd, port->as.port.as.str));
    }
    secd_pprintf(secd, p, ">");
}

