#ifndef __SECD_ENV_H__
#define __SECD_ENV_H__

#define SECD_FAKEVAR_STDIN  "*stdin*"
#define SECD_FAKEVAR_STDOUT "*stdout*"
#define SECD_FAKEVAR_STDDBG "*stddbg*"
#define SECD_FAKEVAR_MODULE "*module*"

typedef struct {
    const char *name;
    const cell_t *val;
} native_binding_t;

extern const native_binding_t native_functions[];

cell_t * make_frame_of_natives(secd_t *secd);

void print_env(secd_t *secd);
void init_env(secd_t *secd);

cell_t *setup_frame(secd_t *secd, cell_t *argnames, cell_t *argsvals, cell_t *env);
cell_t *secd_insert_in_frame(secd_t *secd, cell_t *frame, cell_t *sym, cell_t *val);

cell_t *lookup_env(secd_t *secd, const char *symbol, cell_t **symc);
cell_t *lookup_symenv(secd_t *secd, const char *symbol);

#endif //__SECD_ENV_H__
