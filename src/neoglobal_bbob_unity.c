/*
 * Optional COCO BBOB benchmark runner. This binary uses the same optimizer
 * implementation as the Lua application, with COCO as its objective provider.
 */
#define CFD_LIB_IMPLEMENTATION

#include "problem.c"
#include "sampling.c"
#include "clustering.c"
#include "local_search.c"
#include "optimizer.c"
#include "bbob_main.c"
