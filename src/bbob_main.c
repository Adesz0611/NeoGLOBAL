#include <neoglobal/optimizer.h>

#include <coco.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    coco_problem_t *problem;
} NeoGlobal_BBOB_Evaluator;

static b32 bbob_evaluate(
    const NeoGlobal_Real *x,
    u32 dimension,
    NeoGlobal_Real *result,
    void *userdata,
    char *error,
    size_t error_size
)
{
    (void)dimension;
    (void)error;
    (void)error_size;

    NeoGlobal_BBOB_Evaluator *evaluator = userdata;
    double objective = 0.0;
    coco_evaluate_function(evaluator->problem, x, &objective);
    *result = (NeoGlobal_Real)objective;
    return true;
}

static b32 parse_u64(const char *text, u64 *value) {
    if (text == NULL || text[0] == '\0' || text[0] == '-')
        return false;

    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
        return false;

    *value = (u64)parsed;
    return true;
}

static b32 is_valid_algorithm_name(const char *name) {
    if (name == NULL || !isalnum((unsigned char)name[0]))
        return false;

    size_t length = 0;
    for (const unsigned char *c = (const unsigned char *)name; *c != '\0'; ++c) {
        if (!isalnum(*c) && *c != '-' && *c != '_' && *c != '.')
            return false;
        if (++length > 64)
            return false;
    }
    return true;
}

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Usage: %s <function:1-24> <dimension> <instance> <max-evaluations> [seed [algorithm-name]]\n"
        "Example: %s 1 2 1 1000 1 NeoGLOBAL-baseline\n",
        program,
        program
    );
}

int main(int argc, char **argv) {
    if (argc < 5 || argc > 7) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    u64 function_index = 0;
    u64 dimension_arg = 0;
    u64 instance_index = 0;
    u64 max_evaluations = 0;
    u64 run_seed = 0;
    const char *algorithm_name = "NeoGLOBAL-baseline";
    if (
        !parse_u64(argv[1], &function_index) || function_index < 1 || function_index > 24 ||
        !parse_u64(argv[2], &dimension_arg) || dimension_arg == 0 || dimension_arg > UINT32_MAX ||
        !parse_u64(argv[3], &instance_index) || instance_index == 0 ||
        !parse_u64(argv[4], &max_evaluations) || max_evaluations == 0 ||
        (argc >= 6 && !parse_u64(argv[5], &run_seed)) ||
        (argc == 7 && !is_valid_algorithm_name(argv[6]))
    ) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 7)
        algorithm_name = argv[6];

    /* COCO's public C API evaluates double vectors, so BBOB stays f64 even
       when the application's default build is configured for f32. */
    _Static_assert(sizeof(NeoGlobal_Real) == sizeof(double), "BBOB runner must use f64");

    char suite_options[160];
    snprintf(
        suite_options,
        sizeof(suite_options),
        "function_indices: %" PRIu64 " instance_indices: %" PRIu64 " dimensions: %" PRIu64,
        function_index,
        instance_index,
        dimension_arg
    );

    char output_folder[256];
    snprintf(
        output_folder,
        sizeof(output_folder),
        "result_folder: results/neoglobal_bbob_f%02" PRIu64 "_d%" PRIu64 "_i%" PRIu64 " algorithm_name: %s",
        function_index,
        dimension_arg,
        instance_index,
        algorithm_name
    );

    coco_set_log_level("warning");
    coco_suite_t *suite = coco_suite("bbob", "", suite_options);
    coco_observer_t *observer = coco_observer("bbob", output_folder);
    if (suite == NULL || observer == NULL) {
        fprintf(stderr, "NeoGLOBAL BBOB error: COCO could not create the suite or observer\n");
        if (observer != NULL)
            coco_observer_free(observer);
        if (suite != NULL)
            coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    coco_problem_t *coco_problem = coco_suite_get_next_problem(suite, observer);
    if (coco_problem == NULL) {
        fprintf(stderr, "NeoGLOBAL BBOB error: COCO returned no problem for the requested selection\n");
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    u32 dimension = (u32)coco_problem_get_dimension(coco_problem);
    if (dimension != (u32)dimension_arg) {
        fprintf(stderr, "NeoGLOBAL BBOB error: selected COCO problem has an unexpected dimension\n");
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    CFD_Arena arena;
    if (!cfd_arena_init(&arena, GB(4))) {
        fprintf(stderr, "NeoGLOBAL BBOB error: failed to initialize memory arena\n");
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    NeoGlobal_Problem *problem = cfd_arena_push_type(&arena, NeoGlobal_Problem);
    NeoGlobal_BBOB_Evaluator *evaluator = cfd_arena_push_type(&arena, NeoGlobal_BBOB_Evaluator);
    NeoGlobal_Real *lower = cfd_arena_push_array(&arena, NeoGlobal_Real, dimension);
    NeoGlobal_Real *upper = cfd_arena_push_array(&arena, NeoGlobal_Real, dimension);
    const double *coco_lower = coco_problem_get_smallest_values_of_interest(coco_problem);
    const double *coco_upper = coco_problem_get_largest_values_of_interest(coco_problem);
    if (problem == NULL || evaluator == NULL || lower == NULL || upper == NULL) {
        fprintf(stderr, "NeoGLOBAL BBOB error: out of memory while creating the problem\n");
        cfd_arena_log_usage("NeoGLOBAL BBOB", &arena);
        cfd_arena_destroy(&arena);
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    for (u32 i = 0; i < dimension; ++i) {
        lower[i] = (NeoGlobal_Real)coco_lower[i];
        upper[i] = (NeoGlobal_Real)coco_upper[i];
    }

    evaluator->problem = coco_problem;
    problem->name = (char *)coco_problem_get_name(coco_problem);
    problem->dimension = dimension;
    problem->random_seed = argc >= 6
        ? run_seed
        : UINT64_C(0x4e454f474c4f4241)
            ^ function_index ^ (instance_index << 16) ^ ((u64)dimension << 32);
    problem->max_evaluations = max_evaluations;
    /* Keep sampling batches below typical budgets so the optimizer has room
       to launch local searches between sampling/reduction rounds. */
    problem->samples_per_iteration = 100;
    problem->reduced_samples = 20;
    problem->alpha = 0.5;
    problem->local_search_max_evaluations = 1000;
    problem->local_search_relative_tolerance = 1e-6;
    problem->lower = lower;
    problem->upper = upper;
    problem->evaluator_type = NEOGLOBAL_EVALUATOR_NATIVE;
    problem->evaluate = bbob_evaluate;
    problem->evaluator_data = evaluator;

    printf("NeoGLOBAL COCO BBOB run\n");
    printf("  Problem:      %s\n", problem->name);
    printf("  Dimension:    %u\n", dimension);
    printf("  Evaluations:  %" PRIu64 "\n", max_evaluations);
    printf("  Seed:         %" PRIu64 "\n", problem->random_seed);
    printf("  Algorithm:    %s\n", algorithm_name);
    printf("  Precision:    f64 (64-bit)\n");

    char error[512] = {0};
    NeoGlobal_Result result;
    struct timespec start_time;
    struct timespec end_time;
    timespec_get(&start_time, TIME_UTC);
    b32 succeeded = neoglobal_optimize(&arena, problem, &result, error, sizeof(error));
    timespec_get(&end_time, TIME_UTC);
    if (!succeeded) {
        fprintf(stderr, "NeoGLOBAL BBOB optimization error:\n%s\n", error);
        cfd_arena_log_usage("NeoGLOBAL BBOB", &arena);
        cfd_arena_destroy(&arena);
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    printf("  Best f(x):    %.17g\n", (double)result.best.objective);
    printf("  Best x:       [");
    for (u32 i = 0; i < dimension; ++i) {
        if (i != 0)
            printf(", ");
        printf("%.17g", (double)result.best.x[i]);
    }
    printf("]\n");
    printf("  Evaluations:  %" PRIu64 "\n", result.evaluations);
    f64 elapsed_seconds = (f64)(end_time.tv_sec - start_time.tv_sec)
        + (f64)(end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("  Elapsed:      %.6f seconds\n", elapsed_seconds);
    printf("  Iterations:   %" PRIu64 "\n", result.iterations);
    printf("  Local search: %" PRIu64 "\n", result.local_searches);
    printf("  Clusters:     %" PRIu64 "\n", result.clusters);

    cfd_arena_log_usage("NeoGLOBAL BBOB", &arena);
    cfd_arena_destroy(&arena);
    coco_observer_free(observer);
    coco_suite_free(suite);
    return EXIT_SUCCESS;
}
