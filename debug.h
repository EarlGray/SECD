#ifndef __SECD_DEBUGH__
#define __SECD_DEBUGH__

#include "conf.h"

#include <stdio.h>

#if (MEMDEBUG)
# define memdebugf(...) printf(__VA_ARGS__)
# if (MEMTRACE)
#  define memtracef(...) printf(__VA_ARGS__)
# else
#  define memtracef(...)
# endif
#else
# define memdebugf(...)
# define memtracef(...)
#endif

#if (CTRLDEBUG)
# define ctrldebugf(...) printf(__VA_ARGS__)
#else
# define ctrldebugf(...)
#endif

#if (ENVDEBUG)
# define envdebugf(...) printf(__VA_ARGS__)
#else
# define envdebugf(...)
#endif

#ifndef __unused
# define __unused __attribute__((unused))
#endif


#endif //__SECD_DEBUGH__
