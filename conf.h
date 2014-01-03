#ifndef __SECD_CONF_H___
#define __SECD_CONF_H___

#define N_CELLS     256 * 1024
#define SECD_ALIGN  4

#define TAILRECURSION 1
#define CASESENSITIVE 0

#define EOF_OBJ     "#<eof>"
#define DONT_FREE_THIS  INTPTR_MAX/2

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

#endif //__SECD_CONF_H___

