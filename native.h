#ifndef __SECD_NATIVE_H__
#define __SECD_NATIVE_H__

typedef struct {
    cell_t *sym;
    cell_t *val;
} binding_t;

cell_t * make_frame_of_natives(secd_t *secd);

#endif //__SECD_NATIVE_H__
