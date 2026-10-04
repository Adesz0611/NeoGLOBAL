#ifndef NEOGLOBAL_LOCAL_SEARCH_H
#define NEOGLOBAL_LOCAL_SEARCH_H

#include <neoglobal/sampling.h>

/* Bounded coordinate pattern search used as the initial local-search method.
   The method is a project choice: the ParallelGlobal paper leaves local search
   abstract. The returned point is stored in arena-owned memory. */
b32 neoglobal_local_search_coordinate(
    CFD_Arena *arena,
    const NeoGlobal_Problem *problem,
    const NeoGlobal_Sample *start,
    NeoGlobal_Real *workspace_x,
    f64 *workspace_steps,
    u64 max_evaluations,
    f64 relative_tolerance,
    NeoGlobal_Sample *result,
    u64 *evaluations_used,
    char *error,
    size_t error_size
);

#endif
