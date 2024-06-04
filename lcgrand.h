/* The following 3 declarations are for use of the random-number generator
   lcgrand and the associated functions lcgrandst and lcgrandgt for seed
   management.  This file (named lcgrand.h) should be included in any program
   using these functions by executing
       #include "lcgrand.h"
   before referencing the functions. */

#ifndef _LCGRAND_H_
#define _LCGRAND_H_

#ifdef __cplusplus
extern "C" {
#endif

extern float lcgrand(int stream);
extern void  lcgrandst(long zset, int stream);
extern long  lcgrandgt(int stream);

#ifdef __cplusplus
}
#endif

#endif
