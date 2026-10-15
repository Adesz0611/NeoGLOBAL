#include <coco.h>
#include <cmaes_interface.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool parse_u64(const char *text, uint64_t *value) {
    if (text == NULL || text[0] == '\0' || text[0] == '-')
        return false;

    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
        return false;

    *value = (uint64_t)parsed;
    return true;
}

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Usage: %s <function:1-24> <dimension> <instance> <max-evaluations> [seed]\n"
        "Example: %s 1 2 1 1000 1\n",
        program,
        program
    );
}

static bool is_feasible(
    const double *x,
    const double *lower,
    const double *upper,
    size_t dimension
)
{
    for (size_t i = 0; i < dimension; ++i) {
        if (x[i] < lower[i] || x[i] > upper[i])
            return false;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5 && argc != 6) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint64_t function_index = 0;
    uint64_t dimension_arg = 0;
    uint64_t instance_index = 0;
    uint64_t max_evaluations = 0;
    uint64_t seed_arg = 1;
    if (
        !parse_u64(argv[1], &function_index) || function_index < 1 || function_index > 24 ||
        !parse_u64(argv[2], &dimension_arg) || dimension_arg == 0 || dimension_arg > INT_MAX ||
        !parse_u64(argv[3], &instance_index) || instance_index == 0 ||
        !parse_u64(argv[4], &max_evaluations) || max_evaluations == 0 ||
        (argc == 6 && !parse_u64(argv[5], &seed_arg)) ||
        seed_arg == 0 || seed_arg > 2000000000ULL
    ) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    size_t dimension = (size_t)dimension_arg;
    char suite_options[160];
    snprintf(
        suite_options,
        sizeof(suite_options),
        "function_indices: %" PRIu64 " instance_indices: %" PRIu64 " dimensions: %" PRIu64,
        function_index,
        instance_index,
        dimension_arg
    );

    char observer_options[192];
    snprintf(
        observer_options,
        sizeof(observer_options),
        "result_folder: results/cmaes_bbob_f%02" PRIu64 "_d%" PRIu64 "_i%" PRIu64
        " algorithm_name: CMA-ES",
        function_index,
        dimension_arg,
        instance_index
    );

    coco_set_log_level("warning");
    coco_suite_t *suite = coco_suite("bbob", "", suite_options);
    coco_observer_t *observer = coco_observer("bbob", observer_options);
    if (suite == NULL || observer == NULL) {
        fprintf(stderr, "CMA-ES BBOB error: COCO could not create the suite or observer\n");
        if (observer != NULL)
            coco_observer_free(observer);
        if (suite != NULL)
            coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    coco_problem_t *problem = coco_suite_get_next_problem(suite, observer);
    if (problem == NULL || coco_problem_get_dimension(problem) != dimension) {
        fprintf(stderr, "CMA-ES BBOB error: COCO returned no matching problem\n");
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    const double *lower = coco_problem_get_smallest_values_of_interest(problem);
    const double *upper = coco_problem_get_largest_values_of_interest(problem);
    double *initial_solution = coco_allocate_vector(dimension);
    double *initial_sigma = coco_allocate_vector(dimension);
    if (initial_solution == NULL || initial_sigma == NULL) {
        fprintf(stderr, "CMA-ES BBOB error: failed to allocate initialization vectors\n");
        coco_free_memory(initial_solution);
        coco_free_memory(initial_sigma);
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < dimension; ++i) {
        initial_solution[i] = lower[i] + (upper[i] - lower[i]) / 2.0;
        initial_sigma[i] = (upper[i] - lower[i]) / 6.0;
    }

    cmaes_t cma;
    memset(&cma, 0, sizeof(cma));
    cmaes_init_para(&cma, (int)dimension, initial_solution, initial_sigma,
                    (long)seed_arg, 0, "no");
    cma.sp.filename = strdup("no");
    cma.sp.stopMaxFunEvals = (double)max_evaluations;
    double *fitness = cmaes_init_final(&cma);
    size_t population_size = (size_t)cma.sp.lambda;

    if (max_evaluations < population_size) {
        fprintf(
            stderr,
            "CMA-ES BBOB error: evaluation budget must be at least one population (%zu)\n",
            population_size
        );
        cmaes_exit(&cma);
        coco_free_memory(initial_solution);
        coco_free_memory(initial_sigma);
        coco_observer_free(observer);
        coco_suite_free(suite);
        return EXIT_FAILURE;
    }

    printf("CMA-ES COCO BBOB run\n");
    printf("  Problem:      %s\n", coco_problem_get_name(problem));
    printf("  Dimension:    %zu\n", dimension);
    printf("  Evaluations:  %" PRIu64 "\n", max_evaluations);
    printf("  Population:   %zu\n", population_size);
    printf("  Seed:         %" PRIu64 "\n", seed_arg);

    struct timespec start_time;
    struct timespec end_time;
    timespec_get(&start_time, TIME_UTC);

    double *const *population = NULL;
    while (
        cma.countevals + population_size <= (double)max_evaluations &&
        cmaes_TestForTermination(&cma) == NULL
    ) {
        population = cmaes_SamplePopulation(&cma);
        for (size_t i = 0; i < population_size; ++i) {
            while (!is_feasible(population[i], lower, upper, dimension))
                cmaes_ReSampleSingle(&cma, (int)i);
            coco_evaluate_function(problem, population[i], &fitness[i]);
        }
        cmaes_UpdateDistribution(&cma, fitness);
    }

    timespec_get(&end_time, TIME_UTC);

    uint64_t evaluations = (uint64_t)cma.countevals;
    const double *best_x = cmaes_GetPtr(&cma, "xbestever");
    double best_objective = cmaes_Get(&cma, "fbestever");
    printf("  Best f(x):    %.17g\n", best_objective);
    printf("  Best x:       [");
    for (size_t i = 0; i < dimension; ++i) {
        if (i != 0)
            printf(", ");
        printf("%.17g", best_x[i]);
    }
    printf("]\n");
    printf("  Evaluations:  %" PRIu64 "\n", evaluations);
    double elapsed_seconds = (double)(end_time.tv_sec - start_time.tv_sec)
        + (double)(end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("  Elapsed:      %.6f seconds\n", elapsed_seconds);
    printf("  Generations:  %.0f\n", cma.gen);

    cmaes_exit(&cma);
    coco_free_memory(initial_solution);
    coco_free_memory(initial_sigma);
    coco_observer_free(observer);
    coco_suite_free(suite);
    return EXIT_SUCCESS;
}
