#ifndef __SECD_OPS_H__
#define __SECD_OPS_H__


typedef struct {
    const char *name;
    secd_opfunc_t fun;
    int args;       // takes 'args' control cells after the opcode
    int stackuse;   // changes of stack size
} opcode_t;

extern const opcode_t opcode_table[];
extern size_t opcode_count(void);

cell_t * compile_ctrl(secd_t *secd, cell_t **ctrl);

int secdop_by_name(const char *name);

#endif //__SECD_OPS_H__
