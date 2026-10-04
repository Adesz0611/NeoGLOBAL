#ifndef NEOGLOBAL_SAMPLING_H
#define NEOGLOBAL_SAMPLING_H

#include <neoglobal/problem.h>

typedef struct {
    NeoGlobal_Real *x;
    NeoGlobal_Real objective;
} NeoGlobal_Sample;

typedef struct {
    NeoGlobal_Sample *items;
    u64 count;
    u64 capacity;
} NeoGlobal_Sample_Pool;

b32 neoglobal_sample_pool_init(
    CFD_Arena *arena,
    u64 capacity,
    NeoGlobal_Sample_Pool *pool
);

/* Fills x with one uniformly sampled point from the problem's box bounds.
   The caller must seed rng before use. */
b32 neoglobal_sample_uniform(
    const NeoGlobal_Problem *problem,
    CFD_Rng *rng,
    NeoGlobal_Real *x
);

/* Samples a point, evaluates it, and stores its objective in sample. */
b32 neoglobal_sample_random_evaluate(
    const NeoGlobal_Problem *problem,
    CFD_Rng *rng,
    NeoGlobal_Sample *sample,
    char *error,
    size_t error_size
);

/* Appends a successfully evaluated sample to a caller-allocated pool. */
b32 neoglobal_sample_pool_add(
    NeoGlobal_Sample_Pool *pool,
    const NeoGlobal_Sample *sample
);

/*
 * Implements the Global sampling reduction step. The caller first appends
 * newly sampled and evaluated entries to pool. This sorts the pool by
 * objective, copies the best reduced_count entries to selected, and removes
 * them from pool. selected must have room for reduced_count entries and must
 * not overlap pool->items. Sample coordinate storage remains caller-owned.
 */
b32 neoglobal_sample_pool_take_best(
    NeoGlobal_Sample_Pool *pool,
    u32 reduced_count,
    NeoGlobal_Sample *selected,
    u32 selected_capacity
);

/* Runs one configured serial sampling/reduction iteration. */
b32 neoglobal_sampling_iteration(
    CFD_Arena *arena,
    const NeoGlobal_Problem *problem,
    CFD_Rng *rng,
    NeoGlobal_Sample_Pool *pool,
    NeoGlobal_Sample *selected,
    u32 selected_capacity,
    u32 *selected_count,
    u64 *evaluation_count,
    char *error,
    size_t error_size
);

#endif
