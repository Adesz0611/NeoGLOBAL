#include <neoglobal/optimizer.h>

#include <stdio.h>
#include <string.h>

static void neoglobal_optimizer_set_error(
    char *error,
    size_t error_size,
    const char *message
)
{
    if (error != NULL && error_size > 0)
        snprintf(error, error_size, "%s", message);
}

static void neoglobal_optimizer_update_best(
    NeoGlobal_Result *result,
    const NeoGlobal_Sample *sample
)
{
    if (result->best.x == NULL || sample->objective < result->best.objective)
        result->best = *sample;
}

b32 neoglobal_optimize(
    CFD_Arena *arena,
    const NeoGlobal_Problem *problem,
    NeoGlobal_Result *result,
    char *error,
    size_t error_size
)
{
    if (
        arena == NULL || arena->buffer == NULL ||
        problem == NULL || result == NULL ||
        problem->dimension == 0 ||
        problem->max_evaluations == 0 ||
        problem->samples_per_iteration == 0 ||
        problem->reduced_samples == 0 ||
        problem->reduced_samples > problem->samples_per_iteration ||
        problem->local_search_max_evaluations == 0 ||
        problem->local_search_relative_tolerance <= 0.0 ||
        problem->local_search_relative_tolerance >= 1.0 ||
        problem->max_evaluations > UINT64_MAX / 2
    ) {
        neoglobal_optimizer_set_error(error, error_size, "invalid optimizer arguments or settings");
        return false;
    }

    memset(result, 0, sizeof(*result));

    NeoGlobal_Sample_Pool pool;
    if (!neoglobal_sample_pool_init(arena, problem->max_evaluations, &pool)) {
        neoglobal_optimizer_set_error(error, error_size, "failed to allocate sample pool");
        return false;
    }

    NeoGlobal_Sample *reduced = cfd_arena_push_array(
        arena,
        NeoGlobal_Sample,
        problem->reduced_samples
    );
    if (reduced == NULL) {
        neoglobal_optimizer_set_error(error, error_size, "failed to allocate reduced sample buffer");
        return false;
    }

    NeoGlobal_Real *local_workspace_x = cfd_arena_push_array(
        arena,
        NeoGlobal_Real,
        problem->dimension
    );
    f64 *local_workspace_steps = cfd_arena_push_array(
        arena,
        f64,
        problem->dimension
    );
    if (local_workspace_x == NULL || local_workspace_steps == NULL) {
        neoglobal_optimizer_set_error(error, error_size, "failed to allocate local-search workspace");
        return false;
    }

    NeoGlobal_Cluster_Set clusters;
    if (!neoglobal_cluster_set_init(
            arena,
            problem->max_evaluations,
            problem->max_evaluations * 2,
            &clusters
        )) {
        neoglobal_optimizer_set_error(error, error_size, "failed to allocate cluster storage");
        return false;
    }

    CFD_Rng rng;
    cfd_rng_seed(&rng, problem->random_seed);

    while (result->evaluations < problem->max_evaluations) {
        u32 reduced_count = 0;
        if (!neoglobal_sampling_iteration(
                arena,
                problem,
                &rng,
                &pool,
                reduced,
                problem->reduced_samples,
                &reduced_count,
                &result->evaluations,
                error,
                error_size
            )) {
            return false;
        }
        ++result->iterations;

        for (u32 i = 0; i < reduced_count; ++i)
            neoglobal_optimizer_update_best(result, &reduced[i]);

        while (reduced_count > 0 && result->evaluations < problem->max_evaluations) {
            if (!neoglobal_clusters_assign_reduced(
                    problem,
                    &clusters,
                    reduced,
                    &reduced_count,
                    error,
                    error_size
                )) {
                return false;
            }

            if (reduced_count == 0)
                break;

            NeoGlobal_Sample start = reduced[0];
            NeoGlobal_Sample local_minimum;
            u64 local_evaluations = 0;
            u64 remaining_evaluations = problem->max_evaluations - result->evaluations;
            u64 local_budget = problem->local_search_max_evaluations;
            if (local_budget > remaining_evaluations)
                local_budget = remaining_evaluations;

            b32 local_search_succeeded = neoglobal_local_search_coordinate(
                arena,
                problem,
                &start,
                local_workspace_x,
                local_workspace_steps,
                local_budget,
                problem->local_search_relative_tolerance,
                &local_minimum,
                &local_evaluations,
                error,
                error_size
            );
            result->evaluations += local_evaluations;
            if (!local_search_succeeded)
                return false;

            neoglobal_optimizer_update_best(result, &local_minimum);

            if (!neoglobal_clusters_record_local_minimum(
                    problem,
                    &clusters,
                    &start,
                    &local_minimum,
                    reduced_count,
                    error,
                    error_size
                )) {
                return false;
            }
            ++result->local_searches;

            memmove(
                reduced,
                reduced + 1,
                (size_t)(reduced_count - 1) * sizeof(*reduced)
            );
            --reduced_count;
        }
    }

    result->clusters = clusters.count;
    return result->best.x != NULL;
}
