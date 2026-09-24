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

    NeoGlobal_Problem problem;

    char error[512];

    if (
        !neoglobal_problem_load_lua(
            argv[1],
            &problem,
            error,
            sizeof(error)
        )
    ) {
        fprintf(
            stderr,
            "NeoGLOBAL configuration error:\n%s\n",
            error
        );

        return EXIT_FAILURE;
    }

    printf(
        "NeoGLOBAL\n"
        "=========\n\n"
    );

    printf(
        "Problem:    %s\n",
        problem.name
    );

    printf(
        "Dimensions: %u\n",
        problem.dimension
    );

    printf(
        "Evaluator:  %s\n",
        neoglobal_evaluator_type_name(
            problem.evaluator_type
        )
    );

    printf("\nSearch space:\n");

    for (u32 i = 0; i < problem.dimension; ++i) {
        printf(
            "  x[%u] = [%g, %g]\n",
            i + 1,
            (double)problem.lower[i],
            (double)problem.upper[i]
        );
    }

    /*
     * For now, evaluate the center of
     * the search space as an end-to-end test.
     */
    NeoGlobal_Real *x = malloc(sizeof(*x) * problem.dimension);

    if (x == NULL) {
        fprintf(stderr, "NeoGLOBAL error: out of memory\n");

        neoglobal_problem_destroy(&problem);

        return EXIT_FAILURE;
    }

    for (u32 i = 0; i < problem.dimension; ++i) {
        x[i] =
            problem.lower[i]
            + (
                problem.upper[i]
                - problem.lower[i]
            )
            * NEOGLOBAL_REAL_CAST(0.5);
    }

    NeoGlobal_Real result;

    if (
        !neoglobal_problem_evaluate(
            &problem,
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

        free(x);

        neoglobal_problem_destroy(
            &problem
        );

        return EXIT_FAILURE;
    }

    printf("\nSearch-space midpoint:\n");

    printf("  x = [");

    for (u32 i = 0; i < problem.dimension; ++i) {
        if (i != 0)
            printf(", ");

        printf("%g", (double)x[i]);
    }

    printf("]\n");

    printf("  f(x) = %.17g\n", (double)result);

    free(x);

    neoglobal_problem_destroy(
        &problem
    );

    return EXIT_SUCCESS;
}
