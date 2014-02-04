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

cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode) {
    FILE *f = fopen(fname, mode);
    if (!f)
        return new_error(secd, "secd_fopen('%s'): %s\n", fname, strerror(errno));

    cell_t *cmdport = new_fileport(secd, f, mode);
    assert_cellf(cmdport, "secd_fopen: failed to create port for '%s'\n", fname);
    return cmdport;
}

/*
 * Port-reading
 */
int secd_getc(secd_t *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_getc: not a port");
    io_assert(is_input(port), "secd_getc: not an input port");

    if (port->as.port.file) {
        return fgetc(port->as.port.as.file);
    } else {
        cell_t *str = port->as.port.as.str;
        size_t size = mem_size(secd, str); 
        if (str->as.str.offset >= (int)size)
            return EOF;

        char c = strmem(str)[str->as.str.offset];
        ++str->as.str.offset;
        return (int)c;
    }
}

/*
 * Port-printing
 */
int secd_vprintf(secd_t *secd, cell_t *port, const char *format, va_list ap) {
    io_assert(cell_type(port) == CELL_PORT, "vpprintf: not a port");
    io_assert(is_output(port), "vpprintf: not an output port");
    int ret;

    if (port->as.port.file) {
        ret = vfprintf(port->as.port.as.file, format, ap);
    } else {
        cell_t *str = port->as.port.as.str;
        char *mem = strmem(str);
        size_t offset = str->as.str.offset;
        size_t size = mem_size(secd, str) - offset;
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

