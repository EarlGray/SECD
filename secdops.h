#ifndef __SECD_OPS_H__
#define __SECD_OPS_H__


typedef struct {
    const cell_t *sym;
    const cell_t *val;
    int args;       // takes 'args' control cells after the opcode
} opcode_t;

extern const opcode_t opcode_table[];
extern size_t opcode_count(void);

#endif //__SECD_OPS_H__
