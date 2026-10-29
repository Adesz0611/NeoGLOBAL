#ifndef NEOGLOBAL_LOCAL_SEARCH_H
#define NEOGLOBAL_LOCAL_SEARCH_H

#include <neoglobal/sampling.h>

typedef enum {
    NEOGLOBAL_LOCAL_SEARCH_ERROR,
    NEOGLOBAL_LOCAL_SEARCH_CONVERGED,
    NEOGLOBAL_LOCAL_SEARCH_BUDGET_EXHAUSTED,
} NeoGlobal_Local_Search_Status;

/* Bounded coordinate pattern search used as the initial local-search method.
   The method is a project choice: the ParallelGlobal paper leaves local search
   abstract. CONVERGED means the step tolerance was reached, not a proof of a
   local minimum. Both non-error statuses return the best visited point in
   arena-owned memory. On ERROR, result is unchanged; evaluations_used counts
   attempted evaluator calls, including a failed call. */
NeoGlobal_Local_Search_Status neoglobal_local_search_coordinate(
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
