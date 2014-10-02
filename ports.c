#include "secd.h"
#include "secd_io.h"
#include "memory.h"

#include <string.h>
#include <stdarg.h>

portops_t * secd_strportops();
portops_t * secd_fileportops();

/*
 *  Generic port interface
 */

#define io_assert(cond, ...) \
    if (!(cond)) { \
        errorf(__VA_ARGS__); \
        return -1; \
    }

const char * secd_porttyname(secd_t *secd, int ty) {
    portops_t *ops = secd->portops[ty];
    if (!ops) return "";

    portinfo_func_t pinfo = ops->pinfo;
    if (!pinfo) return "";

    return pinfo(secd, SECD_NIL, NULL);
}

static portops_t * secd_portops(secd_t *secd, cell_t *port) {
    return secd->portops[ port->as.port.type ];
}

static int secd_porttype(secd_t *secd, const char *pname) {
    int i = 0;
    for (i = 0; i < SECD_PORTTYPES_MAX; ++i) {
        if (!strcmp(pname, secd_porttyname(secd, i)))
            return i;
    }
    errorf("porttype %s not found", pname);
    return -1;
}

static cell_t *init_port_mode(secd_t *secd, cell_t *cell, const char *mode) {
    switch (mode[0]) {
      case 'r':
        cell->as.port.input = true;
        if (mode[1] == '+') {
            cell->as.port.output = true;
            ++mode;
        } else
            cell->as.port.output = false;
        if (mode[1] == '\0')
            return cell;
        break;

      case 'w': case 'a':
        cell->as.port.output = true;
        if (mode[1] == '+') {
            cell->as.port.input = true;
            ++mode;
        } else
            cell->as.port.input = false;
        if (mode[1] == '\0')
            return cell;
    }
    // otherwise fail:
    drop_cell(secd, cell);
    errorf("new_fileport: failed to parse mode\n");
    return new_error(secd, SECD_NIL, "new_port: failed to parse mode");
}


cell_t *secd_newport(secd_t *secd, const char *mode, const char *ty, cell_t *params) {
    int pty = secd_porttype(secd, ty);
    assert(0 <= pty && pty < SECD_PORTTYPES_MAX,
           "secd_newport: not a valid porttype");

    cell_t *p = new_port(secd, pty);
    init_port_mode(secd, p, mode);
    secd_popen(secd, p, mode, params);
    assert_cell(p, "secd_newport: failed to create a port");
    return p;
}

cell_t *secd_newport_by_name(secd_t *secd, const char *mode, const char *ty, const char * name) {
    cell_t *fname = share_cell(secd, new_string(secd, name));
    cell_t *p = secd_newport(secd, mode, ty, fname);
    drop_cell(secd, fname);
    return p;
}

cell_t *secd_stdin(secd_t *secd) {
    return secd_newport_by_name(secd, "r", "file", "stdin");
}

cell_t *secd_stdout(secd_t *secd) {
    return secd_newport_by_name(secd, "w", "file", "stdout");
}

cell_t *secd_stderr(secd_t *secd) {
    return secd_newport_by_name(secd, "w", "file", "stderr");
}

cell_t *secd_stddbg(secd_t __unused *secd) {
    return SECD_NIL;
}

cell_t *secd_set_dbg(secd_t *secd, cell_t *dbgport) {
    secd->debug_port = share_cell(secd, dbgport);
    return secd->debug_port;
}


cell_t *secd_port_owns(secd_t *secd, cell_t *p, 
    cell_t **r1, cell_t **r2, cell_t **r3
) {
    assert(is_closed(p), "secd_popen: port is already opened");

    portowns_func_t powns = secd_portops(secd, p)->powns;
    if (powns)
        return powns(secd, p, r1, r2, r3);
    *r1 = *r2 = *r3 = SECD_NIL;
    return p;
}


int secd_popen(secd_t *secd, cell_t *p, const char *mode, cell_t *info) {
    portopen_func_t popen = secd_portops(secd, p)->popen;
    if (popen)
        return popen(secd, p, mode, info);
    return 0;
}

int secd_pclose(secd_t *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_pclose: not a port\n");
    io_assert(!is_closed(port), "secd_pclose: already closed\n");

    int ret = 0;

    portclose_func_t pclose = secd_portops(secd, port)->pclose;
    if (pclose)
        ret = pclose(secd, port);
    
    port->as.port.input = false;
    port->as.port.output = false;
    return ret;
}

int secd_pgetc(secd_t __unused *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_getc: not a port\n");
    io_assert(is_input(port), "secd_getc: not an input port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    portgetc_func_t pgetc = secd_portops(secd, port)->pgetc;
    if (!pgetc) return -1;

    return pgetc(secd, port);
}

size_t secd_pread(secd_t *secd, cell_t *port, char *s, int size) {
    io_assert(cell_type(port) == CELL_PORT, "secd_fread: not a port\n");
    io_assert(is_input(port), "secd_fread: not an input port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    portread_func_t pread = secd_portops(secd, port)->pread;
    if (!pread) return 0;

    return pread(secd, port, size, s);
}

long secd_portsize(secd_t *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_portsize: not a port\n");

    portsize_func_t psize = secd_portops(secd, port)->psize;
    if (!psize) return -1;

    return psize(secd, port);
}

int secd_vpprintf(secd_t *secd, cell_t *port, const char *format, va_list ap) {
    io_assert(cell_type(port) == CELL_PORT, "vpprintf: not a port\n");
    io_assert(is_output(port), "vpprintf: not an output port\n");
    io_assert(!is_closed(port), "secd_getc: port is closed\n");

    portvprintf_func_t vprintf = secd_portops(secd, port)->pvprintf;
    if (!vprintf) return -1;

    return vprintf(secd, port, format, ap);
}

inline int 
secd_pprintf(secd_t *secd, cell_t *port, const char *format, ...) {
    va_list ap = NULL;

    va_start(ap, format);
    int ret = secd_vpprintf(secd, port, format, ap);
    va_end(ap);

    return ret;
}

int secd_printf(secd_t *secd, const char *format, ...) {
    va_list ap = NULL;

    va_start(ap, format);
    int ret = secd_vpprintf(secd, secd->output_port, format, ap);
    va_end(ap);

    return ret;
}

/* print port description into port */
void sexp_pprint_port(secd_t *secd, cell_t *outp, const cell_t *port) {
    secd_pprintf(secd, outp, "##port%s%s@%ld",
                    (is_input(port)? "r" : ""),
                    (is_output(port)? "w" : ""),
                    cell_index(secd, port));
}


int secd_register_porttype(secd_t *secd, portops_t *ops) {
    int i;
    int avail = -1;
    portinfo_func_t pinfo = ops->pinfo;
    io_assert(pinfo, "failed to register porttype: no name");

    const char *newpty = ops->pinfo(secd, NULL, NULL);
    for (i = 0; i < SECD_PORTTYPES_MAX; ++i) {
        if (secd->portops[i] == NULL) {
            avail = i;
            continue;
        }

        int cmp = strcmp(newpty, secd->portops[i]->pinfo(secd, NULL, NULL));
        io_assert(cmp, "failed to register porttype: name already taken");
    }

    io_assert(avail >= 0,
            "failed to register porttype: no slots left (max %d)",
            SECD_PORTTYPES_MAX);
    secd->portops[avail] = ops;
    return avail;
}

void secd_init_ports(secd_t *secd) {
    int i;
    for (i = 0; i < SECD_PORTTYPES_MAX; ++i)
        secd->portops[i] = NULL;

    secd_register_porttype(secd, secd_strportops());
    secd_register_porttype(secd, secd_fileportops());

    secd->input_port = share_cell(secd, secd_stdin(secd));
    secd->output_port = share_cell(secd, secd_stdout(secd));
    secd->debug_port = SECD_NIL;
}

cell_t *secd_pserialize(secd_t *secd, cell_t *port) {
    /* TODO */
    return SECD_NIL;
}

/*
 *  String ports
 */
typedef  struct strport  strport_t;
struct strport {
    cell_t *str;
};

static const char *strport_info(
        secd_t __unused *secd, cell_t __unused *p, cell_t __unused **pinfo
) {
    return "str";
}

static int
strport_open(secd_t *secd, cell_t *p, const char __unused *mode, cell_t *info) {
    strport_t *sp = (strport_t *)p->as.port.data;
    io_assert(sp->str, "strport_open: no string");

    io_assert(cell_type(info) == CELL_STR, "strport_open: not a string");
    sp->str = share_cell(secd, info);
    return 0;
}

static int strport_close(secd_t *secd, cell_t *p) {
    strport_t *sp = (strport_t *)p->as.port.data;
    asserti(sp->str, "strport_close: no string");

    drop_cell(secd, sp->str);
    sp->str = SECD_NIL;
    return 0;
}

static int strport_getc(secd_t __unused *secd, cell_t *p) {
    strport_t *sp = (strport_t *)p->as.port.data;
    asserti(sp->str, "strport_size: no string");

    cell_t *str = sp->str;
    size_t size = mem_size(str);
    if (str->as.str.offset >= (int)size)
        return EOF;

    char c = strmem(str)[str->as.str.offset];
    if (c == '\0')
        return SECD_EOF;
    ++str->as.str.offset;
    return (int)c;
}

static size_t strport_read(secd_t __unused *secd, cell_t *p, size_t count, char *buf) {
    strport_t *sp = (strport_t *)p->as.port.data;
    asserti(sp->str, "strport_size: no string");

    cell_t *str = sp->str;

    size_t size = count;
    size_t srcsize = mem_size(str);
    if (srcsize < count)
        size = srcsize;

    memcpy(buf, strmem(str), size);
    return size;
}

static int strport_vprintf(secd_t __unused *secd, cell_t *p, const char *fmt, va_list va) {
    strport_t *sp = (strport_t *)p->as.port.data;
    asserti(sp->str, "strport_size: no string");

    cell_t *str = sp->str;

    char *mem = strmem(str);
    size_t offset = str->as.str.offset;
    size_t size = mem_size(str) - offset;
    int ret = vsnprintf(mem, size, fmt, va);
    if (ret == (int)size) {
        errorf("vpprintf: string is too small");
        errorf("vpprintf: TODO: resize string");
        return -1;
    }
    return ret;
}

static long strport_size(secd_t __unused *secd, cell_t *p) {
    strport_t *sp = (strport_t *)p->as.port.data;
    asserti(sp->str, "strport_size: no string");

    return mem_size(sp->str);
}

static cell_t *
strport_owns(secd_t __unused *secd, cell_t *p, cell_t **ref1, cell_t **r2, cell_t **r3) {
    strport_t *sp = (strport_t *)p->as.port.data;
    asserti(sp->str, "strport_size: no string");

    *ref1 = arr_meta(arr_mem(sp->str));
    *r2 = *r3 = SECD_NIL;
    return p;
}

portops_t strops = {
    .pinfo = strport_info,
    .popen = strport_open,
    .pgetc = strport_getc,
    .pread = strport_read,
    .pvprintf = strport_vprintf,
    .psize = strport_size,
    .pclose = strport_close,
    .powns = strport_owns,
};

portops_t * secd_strportops() {
    return &strops;
}

/*
 *   File ports
 */

#include <stdio.h>
#include <errno.h>

typedef  struct fileport  fileport_t;
struct fileport {
    FILE *f;
};

static const char * fileport_info(secd_t __unused *secd, 
        cell_t __unused *p, cell_t __unused **pinfo
) {
    return "file";
}

static int fileport_open(secd_t __unused *secd, cell_t * port, const char *mode, cell_t *info) {
    fileport_t *fp = (fileport_t *)port->as.port.data;

    io_assert(cell_type(info) == CELL_STR, "secd_fopen: filename not a string");
    const char *fname = info->as.str.data;

    if (!strcmp(fname, "stdin")) {
        fp->f = stdin;
        return 0;
    } else if (!strcmp(fname, "stdout")) {
        fp->f = stdout;
        return 0;
    } else if (!strcmp(fname, "stderr")) {
        fp->f = stderr;
        return 0;
    }

    FILE *f = fopen(fname, mode);
    io_assert(f, "secd_fopen('%s'): %s\n", fname, strerror(errno));

    fp->f = f;
    return 0;
}

static int fileport_close(secd_t __unused *secd, cell_t *p) {
    fileport_t *fp = (fileport_t *)p->as.port.data;
    io_assert(fp->f, "fileport_close: no file");

    int ret = fclose(fp->f);
    fp->f = NULL;
    return ret;
}

static int fileport_getc(secd_t __unused *secd, cell_t *p) {
    fileport_t *fp = (fileport_t *)p->as.port.data;
    io_assert(fp->f, "fileport_close: no file");

    int c = fgetc(fp->f);
    if (c == EOF)
        return SECD_EOF;
    return c;
}

static size_t fileport_read(secd_t __unused *secd, cell_t *p, size_t count, char *buf) {
    fileport_t *fp = (fileport_t *)p->as.port.data;
    asserti(fp->f, "fileport_close: no file");

    return fread(buf, count, 1, fp->f);;
}

static int fileport_vprintf(secd_t __unused *secd, cell_t *p, const char *fmt, va_list ap) {
    fileport_t *fp = (fileport_t *)p->as.port.data;
    asserti(fp->f, "fileport_close: no file");

    return vfprintf(fp->f, fmt, ap);
}

static long fileport_size(secd_t __unused *secd, cell_t *p) {
    fileport_t *fp = (fileport_t *)p->as.port.data;
    asserti(fp->f, "fileport_size: no file");

    FILE *f = fp->f;
    long curpos = ftell(f);

    if (!fseek(f, 0, SEEK_END))
        return -1; /* file is not seekable */

    long endpos = ftell(f);
    fseek(f, curpos, SEEK_SET);
    return endpos;
}

portops_t fileops = {
    .pinfo = fileport_info,
    .popen = fileport_open,
    .pgetc = fileport_getc,
    .pread = fileport_read,
    .pvprintf = fileport_vprintf,
    .psize = fileport_size,
    .pclose = fileport_close,
};

portops_t * secd_fileportops() {
    return &fileops;
}
