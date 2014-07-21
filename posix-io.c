#include "secd.h"
#include "secd_io.h"
#include "memory.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

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
    if (!f)
        return new_error(secd, "secd_fopen('%s'): %s\n", fname, strerror(errno));

    cell_t *fport = new_fileport(secd, f, mode);
    assert_cellf(fport, "secd_fopen: failed to create port for '%s'\n", fname);
    return fport;
}

#ifdef IO_POSIX_PORTSIZE
    if (port->as.port.file) {
        FILE *f = port->as.port.as.file;
        long curpos = ftell(f);

        if (!fseek(f, 0, SEEK_END))
            return -1; /* file is not seekable */

        long endpos = ftell(f);
        fseek(f, curpos, SEEK_SET);
        return endpos;
#endif

#ifdef IO_POSIX_PORTGETC
    if (port->as.port.file) {
        int c = fgetc(port->as.port.as.file);
        if (c != EOF)
            return c;
        return SECD_EOF;
#endif

/*
 * Port-printing
 */
int secd_vprintf(secd_t __unused *secd, cell_t *port, const char *format, va_list ap) {
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

int secd_printf(secd_t *secd, cell_t *port, const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    int ret = secd_vprintf(secd, port, format, ap);
    va_end(ap);

    return ret;
}

void sexp_print_port(secd_t *secd, const cell_t *port) {
    if (SECD_NIL == port->as.port.as.str) {
        printf("#<closed>");
        return;
    }
    bool in = port->as.port.input;
    bool out = port->as.port.output;
    printf("#<%s%s%s: ", (in ? "input" : ""), (out ? "output" : ""), (in && out ? "/" : ""));

    if (port->as.port.file) {
        printf("file %d", fileno(port->as.port.as.file));
    } else {
        printf("string %ld", cell_index(secd, port->as.port.as.str));
    }
    printf(">");
}

