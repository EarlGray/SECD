#ifndef __SECD_OPS_H__
#define __SECD_OPS_H__

typedef enum {
    SECD_ADD,
    SECD_AP,
    SECD_ATOM,
    SECD_CAR,
    SECD_CDR,
    SECD_CONS,
    SECD_DIV,
    SECD_DUM,
    SECD_EQ,
    SECD_JOIN,
    SECD_LD,
    SECD_LDC,
    SECD_LDF,
    SECD_LEQ,
    SECD_MUL,
    SECD_PRN,
    SECD_RAP,
    SECD_READ,
    SECD_REM,
    SECD_RTN,
    SECD_SEL,
    SECD_STOP,
    SECD_SUB,
    SECD_LAST,
} opindex_t;

typedef struct {
    const cell_t *sym;
    const cell_t *val;
    int args;       // takes 'args' control cells after the opcode
} opcode_t;

extern const opcode_t opcode_table[];

#endif //__SECD_OPS_H__
