#include <neoglobal/sampling.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void neoglobal_sampling_set_error(
    char *error,
    size_t error_size,
    const char *message
)
{
    if (error != NULL && error_size > 0)
        snprintf(error, error_size, "%s", message);
}

static int neoglobal_sample_compare_objective(const void *left, const void *right) {
    const NeoGlobal_Sample *a = left;
    const NeoGlobal_Sample *b = right;

    if (a->objective < b->objective)
        return -1;
    if (a->objective > b->objective)
        return 1;
    return 0;
}

static b32 neoglobal_memory_ranges_overlap(
    const void *a,
    size_t a_size,
    const void *b,
    size_t b_size
)
{
    uintptr_t a_begin = (uintptr_t)a;
    uintptr_t b_begin = (uintptr_t)b;

    if (a_begin <= b_begin)
        return b_begin - a_begin < a_size;

    return a_begin - b_begin < b_size;
}

static b32 neoglobal_sample_array_size_fits(u64 count, size_t element_size) {
    return element_size != 0 && count <= (u64)(SIZE_MAX / element_size);
}

b32 neoglobal_sample_pool_init(
    CFD_Arena *arena,
    u64 capacity,
    NeoGlobal_Sample_Pool *pool
)
{
    if (
        arena == NULL ||
        arena->buffer == NULL ||
        pool == NULL ||
        capacity == 0 ||
        capacity > (u64)(SIZE_MAX / sizeof(*pool->items))
    ) {
        return false;
    }

    NeoGlobal_Sample *items = cfd_arena_push_array(arena, NeoGlobal_Sample, capacity);
    if (items == NULL)
        return false;

    pool->items = items;
    pool->count = 0;
    pool->capacity = capacity;
    return true;
}

b32 neoglobal_sample_uniform(
    const NeoGlobal_Problem *problem,
    CFD_Rng *rng,
    NeoGlobal_Real *x
)
{
    if (problem == NULL || rng == NULL || x == NULL)
        return false;

    if (
        problem->dimension == 0 ||
        problem->lower == NULL ||
        problem->upper == NULL
    ) {
        return false;
    }

    for (u32 i = 0; i < problem->dimension; ++i) {
        if (
            !isfinite((double)problem->lower[i]) ||
            !isfinite((double)problem->upper[i]) ||
            problem->lower[i] >= problem->upper[i]
        ) {
            return false;
        }
    }

    for (u32 i = 0; i < problem->dimension; ++i) {
        NeoGlobal_Real unit_sample;

#ifdef NEOGLOBAL_USE_F32
        unit_sample = (NeoGlobal_Real)cfd_rng_next_f32(rng);
#else
        unit_sample = (NeoGlobal_Real)cfd_rng_next_f64(rng);
#endif

        NeoGlobal_Real lower = problem->lower[i];
        NeoGlobal_Real upper = problem->upper[i];
        NeoGlobal_Real value;

        if (lower < 0 && upper > 0) {
            /* Avoid overflow in (upper - lower) for wide signed ranges. */
            value = lower * (1 - unit_sample) + upper * unit_sample;
        }
        else {
            value = lower + (upper - lower) * unit_sample;
        }

        /* Protect the box invariant against floating-point rounding. */
        if (value < lower)
            value = lower;
        else if (value > upper)
            value = upper;

        x[i] = value;
    }

    return true;
}

b32 neoglobal_sample_random_evaluate(
    const NeoGlobal_Problem *problem,
    CFD_Rng *rng,
    NeoGlobal_Sample *sample,
    char *error,
    size_t error_size
)
{
    if (sample == NULL || sample->x == NULL) {
        neoglobal_sampling_set_error(error, error_size, "sample or sample vector is NULL");
        return false;
    }

    if (!neoglobal_sample_uniform(problem, rng, sample->x)) {
        neoglobal_sampling_set_error(error, error_size, "invalid problem, RNG, or bounds");
        return false;
    }

    if (
        !neoglobal_problem_evaluate(
            problem,
            sample->x,
            &sample->objective,
            error,
            error_size
        )
    ) {
        return false;
    }

    if (!isfinite((double)sample->objective)) {
        neoglobal_sampling_set_error(error, error_size, "evaluator returned a non-finite objective");
        return false;
    }

    return true;
}

b32 neoglobal_sample_pool_add(
    NeoGlobal_Sample_Pool *pool,
    const NeoGlobal_Sample *sample
)
{
    if (
        pool == NULL ||
        pool->items == NULL ||
        sample == NULL ||
        sample->x == NULL ||
        !isfinite((double)sample->objective) ||
        pool->count >= pool->capacity
    ) {
        return false;
    }

    pool->items[pool->count++] = *sample;
    return true;
}

b32 neoglobal_sample_pool_take_best(
    NeoGlobal_Sample_Pool *pool,
    u32 reduced_count,
    NeoGlobal_Sample *selected,
    u32 selected_capacity
)
{
    if (
        pool == NULL ||
        pool->items == NULL ||
        selected == NULL ||
        pool->count > pool->capacity ||
        reduced_count == 0 ||
        reduced_count > pool->count ||
        reduced_count > selected_capacity ||
        !neoglobal_sample_array_size_fits(pool->capacity, sizeof(*pool->items)) ||
        !neoglobal_sample_array_size_fits(selected_capacity, sizeof(*selected)) ||
        neoglobal_memory_ranges_overlap(
            pool->items,
            (size_t)pool->capacity * sizeof(*pool->items),
            selected,
            (size_t)selected_capacity * sizeof(*selected)
        )
    ) {
        return false;
    }

    for (u64 i = 0; i < pool->count; ++i) {
        if (
            pool->items[i].x == NULL ||
            !isfinite((double)pool->items[i].objective)
        ) {
            return false;
        }
    }

    if (!neoglobal_sample_array_size_fits(pool->count, sizeof(*pool->items)))
        return false;

    qsort(
        pool->items,
        (size_t)pool->count,
        sizeof(*pool->items),
        neoglobal_sample_compare_objective
    );

    memcpy(
        selected,
        pool->items,
        (size_t)reduced_count * sizeof(*selected)
    );

    u64 remaining_count = pool->count - reduced_count;
    memmove(
        pool->items,
        pool->items + reduced_count,
        (size_t)remaining_count * sizeof(*pool->items)
    );
    pool->count = remaining_count;

    return true;
}

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
)
{
    if (
        arena == NULL ||
        arena->buffer == NULL ||
        problem == NULL ||
        rng == NULL ||
        pool == NULL ||
        pool->items == NULL ||
        selected == NULL ||
        selected_count == NULL ||
        evaluation_count == NULL ||
        problem->samples_per_iteration == 0 ||
        problem->reduced_samples == 0 ||
        problem->max_evaluations == 0 ||
        pool->count > pool->capacity
    ) {
        neoglobal_sampling_set_error(error, error_size, "invalid sampling iteration arguments");
        return false;
    }

    if (*evaluation_count >= problem->max_evaluations) {
        neoglobal_sampling_set_error(error, error_size, "maximum evaluation count reached");
        return false;
    }

    u64 evaluations_remaining = problem->max_evaluations - *evaluation_count;
    u32 samples_to_generate = problem->samples_per_iteration;
    if ((u64)samples_to_generate > evaluations_remaining)
        samples_to_generate = (u32)evaluations_remaining;

    if (samples_to_generate > pool->capacity - pool->count) {
        neoglobal_sampling_set_error(error, error_size, "sample pool capacity exceeded");
        return false;
    }

    u64 combined_count = pool->count + samples_to_generate;
    u32 to_reduce = problem->reduced_samples;
    if ((u64)to_reduce > combined_count)
        to_reduce = (u32)combined_count;

    if (to_reduce == 0 || to_reduce > selected_capacity) {
        neoglobal_sampling_set_error(error, error_size, "selected sample buffer is too small");
        return false;
    }

    u64 coordinate_count = (u64)samples_to_generate * problem->dimension;
    if (coordinate_count > UINT64_MAX / sizeof(NeoGlobal_Real)) {
        neoglobal_sampling_set_error(error, error_size, "sample vector allocation size overflow");
        return false;
    }

    NeoGlobal_Real *coordinates = cfd_arena_push_array(
        arena,
        NeoGlobal_Real,
        coordinate_count
    );
    if (coordinates == NULL) {
        neoglobal_sampling_set_error(error, error_size, "out of memory allocating sample vectors");
        return false;
    }

    for (u32 i = 0; i < samples_to_generate; ++i) {
        NeoGlobal_Sample sample;
        sample.x = coordinates + (u64)i * problem->dimension;

        if (!neoglobal_sample_random_evaluate(problem, rng, &sample, error, error_size))
            return false;

        ++*evaluation_count;

        if (!neoglobal_sample_pool_add(pool, &sample)) {
            neoglobal_sampling_set_error(error, error_size, "failed to append sample to pool");
            return false;
        }
    }

    if (!neoglobal_sample_pool_take_best(pool, to_reduce, selected, selected_capacity)) {
        neoglobal_sampling_set_error(error, error_size, "failed to reduce sample pool");
        return false;
    }

    *selected_count = to_reduce;
    return true;
}
