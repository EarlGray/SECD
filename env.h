#ifndef __SECD_ENV_H__
#define __SECD_ENV_H__

cell_t * make_frame_of_natives(secd_t *secd);

void print_env(secd_t *secd);
void init_env(secd_t *secd);
cell_t *lookup_env(secd_t *secd, const char *symbol);
cell_t *lookup_symenv(secd_t *secd, const char *symbol);

#endif //__SECD_ENV_H__
