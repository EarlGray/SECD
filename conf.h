#ifndef __SECD_CONF_H___
#define __SECD_CONF_H___

#define N_CELLS     256 * 1024

#define TAILRECURSION 1
#define CASESENSITIVE 0

#define TYPE_BITS  8
#define NREF_BITS  (8 * sizeof(size_t) - TYPE_BITS)

#define DONT_FREE_THIS  (1ul << (8 * sizeof(size_t) - TYPE_BITS - 2))

#if CASESENSITIVE
# define str_eq(s1, s2)  !strcmp(s1, s2)
# define str_cmp(s1, s2) strcmp(s1, s2)
#else
# define str_eq(s1, s2) !strcasecmp(s1, s2)
# define str_cmp(s1, s2) strcasecmp(s1, s2)
#endif

#define MEMDEBUG    0
#define MEMTRACE    0
#define CTRLDEBUG   0
#define ENVDEBUG    0
#define TIMING      0

typedef enum { false, true } bool;

typedef  long  index_t;

#endif //__SECD_CONF_H___

