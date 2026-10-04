#ifndef NEOGLOBAL_OPTIMIZER_H
#define NEOGLOBAL_OPTIMIZER_H

#include <neoglobal/clustering.h>
#include <neoglobal/local_search.h>

typedef struct {
    NeoGlobal_Sample best;
    u64 evaluations;
    u64 iterations;
    u64 local_searches;
    u64 clusters;
} NeoGlobal_Result;

b32 neoglobal_optimize(
    CFD_Arena *arena,
    const NeoGlobal_Problem *problem,
    NeoGlobal_Result *result,
    char *error,
    size_t error_size
);

#endif
