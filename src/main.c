#include <neoglobal/problem.h>

#include <stdio.h>
#include <stdlib.h>

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s <problem.lua>\n", program);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(argv[0]);

        return EXIT_FAILURE;
    }

    CFD_Arena arena;
    if (!cfd_arena_init(&arena, GB(4))) {
        fprintf(stderr, "NeoGLOBAL error: failed to initialize memory arena\n");
        return EXIT_FAILURE;
    }

    NeoGlobal_Problem *problem = cfd_arena_push_type(&arena, NeoGlobal_Problem);
    if (!problem) {
        fprintf(stderr, "NeoGLOBAL error: out of memory\n");
        cfd_arena_log_usage("NeoGLOBAL", &arena);
        cfd_arena_destroy(&arena);
        return EXIT_FAILURE;
    }

    char error[512];

    if (
        !neoglobal_problem_load_lua(
            &arena,
            argv[1],
            problem,
            error,
            sizeof(error)
        )
    ) {
        fprintf(
            stderr,
            "NeoGLOBAL configuration error:\n%s\n",
            error
        );

        cfd_arena_log_usage("NeoGLOBAL", &arena);
        cfd_arena_destroy(&arena);

        return EXIT_FAILURE;
    }

    printf(
        "NeoGLOBAL\n"
        "=========\n\n"
    );

    printf(
        "Problem:    %s\n",
        problem->name
    );

    printf(
        "Dimensions: %u\n",
        problem->dimension
    );

    printf(
        "Evaluator:  %s\n",
        neoglobal_evaluator_type_name(
            problem->evaluator_type
        )
    );

    printf(
        "Precision:  %s (%zu-bit)\n",
        sizeof(NeoGlobal_Real) == sizeof(f32) ? "f32" : "f64",
        sizeof(NeoGlobal_Real) * 8
    );

    printf("\nSearch space:\n");

    for (u32 i = 0; i < problem->dimension; ++i) {
        printf(
            "  x[%u] = [%g, %g]\n",
            i + 1,
            (double)problem->lower[i],
            (double)problem->upper[i]
        );
    }

    /*
     * For now, evaluate the center of
     * the search space as an end-to-end test.
     */
    NeoGlobal_Real *x = cfd_arena_push_array(&arena, NeoGlobal_Real, problem->dimension);

    if (x == NULL) {
        fprintf(stderr, "NeoGLOBAL error: out of memory\n");

        neoglobal_problem_destroy(problem);
        cfd_arena_log_usage("NeoGLOBAL", &arena);
        cfd_arena_destroy(&arena);

        return EXIT_FAILURE;
    }

    for (u32 i = 0; i < problem->dimension; ++i) {
        x[i] =
            problem->lower[i]
            + (
                problem->upper[i]
                - problem->lower[i]
            )
            * NEOGLOBAL_REAL_CAST(0.5);
    }

    NeoGlobal_Real result;

    if (
        !neoglobal_problem_evaluate(
            problem,
            x,
            &result,
            error,
            sizeof(error)
        )
    ) {
        fprintf(
            stderr,
            "NeoGLOBAL evaluator error:\n%s\n",
            error
        );

        neoglobal_problem_destroy(problem);

        cfd_arena_log_usage("NeoGLOBAL", &arena);
        cfd_arena_destroy(&arena);

        return EXIT_FAILURE;
    }

    printf("\nSearch-space midpoint:\n");

    printf("  x = [");

    for (u32 i = 0; i < problem->dimension; ++i) {
        if (i != 0)
            printf(", ");

        printf("%g", (double)x[i]);
    }

    printf("]\n");

    printf("  f(x) = %.17g\n", (double)result);

    neoglobal_problem_destroy(problem);
    cfd_arena_log_usage("NeoGLOBAL", &arena);
    cfd_arena_destroy(&arena);

    return EXIT_SUCCESS;
}
