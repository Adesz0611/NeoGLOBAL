#include <neoglobal/local_search.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

static void neoglobal_local_search_set_error(
    char *error,
    size_t error_size,
    const char *message
)
{
    if (error != NULL && error_size > 0)
        snprintf(error, error_size, "%s", message);
}

static b32 neoglobal_local_search_evaluate(
    const NeoGlobal_Problem *problem,
    NeoGlobal_Real *x,
    NeoGlobal_Real *objective,
    u64 *evaluations_used,
    char *error,
    size_t error_size
)
{
    ++*evaluations_used;
    if (!neoglobal_problem_evaluate(problem, x, objective, error, error_size))
        return false;

    if (!isfinite((double)*objective)) {
        neoglobal_local_search_set_error(error, error_size, "evaluator returned a non-finite objective");
        return false;
    }

    return true;
}

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
)
{
    if (evaluations_used != NULL)
        *evaluations_used = 0;

    if (
        arena == NULL || arena->buffer == NULL ||
        problem == NULL || problem->dimension == 0 ||
        problem->lower == NULL || problem->upper == NULL || problem->evaluate == NULL ||
        start == NULL || start->x == NULL ||
        workspace_x == NULL || workspace_steps == NULL ||
        !isfinite((double)start->objective) ||
        max_evaluations == 0 ||
        !isfinite(relative_tolerance) || relative_tolerance <= 0.0 || relative_tolerance >= 0.5 ||
        result == NULL || evaluations_used == NULL
    ) {
        neoglobal_local_search_set_error(error, error_size, "invalid local-search arguments");
        return NEOGLOBAL_LOCAL_SEARCH_ERROR;
    }

    NeoGlobal_Real *current = workspace_x;
    NeoGlobal_Real *saved_x;
    f64 *steps = workspace_steps;
    memcpy(current, start->x, (size_t)problem->dimension * sizeof(*current));
    NeoGlobal_Real current_objective = start->objective;

    f64 largest_relative_step = 0.0;
    for (u32 i = 0; i < problem->dimension; ++i) {
        f64 lower = (f64)problem->lower[i];
        f64 upper = (f64)problem->upper[i];
        if (
            !isfinite(lower) || !isfinite(upper) || lower >= upper ||
            !isfinite((double)current[i]) ||
            current[i] < problem->lower[i] || current[i] > problem->upper[i]
        ) {
            neoglobal_local_search_set_error(error, error_size, "invalid bounds or start point");
            return NEOGLOBAL_LOCAL_SEARCH_ERROR;
        }

        /* Quarter-span steps, computed without subtracting opposite extreme
           bounds directly. The relative stopping test uses half-span. */
        steps[i] = upper * 0.25 - lower * 0.25;
        f64 half_span = upper * 0.5 - lower * 0.5;
        f64 relative_step = steps[i] / half_span;
        if (relative_step > largest_relative_step)
            largest_relative_step = relative_step;
    }

    while (largest_relative_step > relative_tolerance && *evaluations_used < max_evaluations) {
        b32 improved = false;

        for (u32 i = 0; i < problem->dimension; ++i) {
            for (
                s32 direction = 1;
                direction >= -1;
                direction -= 2
            ) {
                f64 lower = (f64)problem->lower[i];
                f64 upper = (f64)problem->upper[i];
                f64 trial_value = (f64)current[i] + (f64)direction * steps[i];
                if (trial_value < lower)
                    trial_value = lower;
                else if (trial_value > upper)
                    trial_value = upper;

                NeoGlobal_Real trial_coordinate = (NeoGlobal_Real)trial_value;
                if (trial_coordinate == current[i])
                    continue;

                /* A partial poll cannot justify reducing the step. */
                if (*evaluations_used >= max_evaluations)
                    goto save_result;

                NeoGlobal_Real old_coordinate = current[i];
                current[i] = trial_coordinate;

                NeoGlobal_Real trial_objective;
                if (!neoglobal_local_search_evaluate(
                        problem,
                        current,
                        &trial_objective,
                        evaluations_used,
                        error,
                        error_size
                    )) {
                    return NEOGLOBAL_LOCAL_SEARCH_ERROR;
                }

                if (trial_objective < current_objective) {
                    current_objective = trial_objective;
                    improved = true;
                    break;
                }

                current[i] = old_coordinate;
            }
        }

        if (!improved) {
            largest_relative_step = 0.0;
            for (u32 i = 0; i < problem->dimension; ++i) {
                steps[i] *= 0.5;
                f64 half_span =
                    (f64)problem->upper[i] * 0.5 - (f64)problem->lower[i] * 0.5;
                f64 relative_step = steps[i] / half_span;
                if (relative_step > largest_relative_step)
                    largest_relative_step = relative_step;
            }
        }
    }

save_result:
    saved_x = cfd_arena_push_array(arena, NeoGlobal_Real, problem->dimension);
    if (saved_x == NULL) {
        neoglobal_local_search_set_error(error, error_size, "out of memory saving local-search result");
        return NEOGLOBAL_LOCAL_SEARCH_ERROR;
    }

    memcpy(saved_x, current, (size_t)problem->dimension * sizeof(*saved_x));
    result->x = saved_x;
    result->objective = current_objective;
    return largest_relative_step <= relative_tolerance
        ? NEOGLOBAL_LOCAL_SEARCH_CONVERGED
        : NEOGLOBAL_LOCAL_SEARCH_BUDGET_EXHAUSTED;
}
