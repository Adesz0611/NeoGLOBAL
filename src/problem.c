#include <neoglobal/problem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void problem_set_error(char *error, size_t error_size, const char *message) {
    if (error == NULL || error_size == 0)
        return;

    snprintf(error, error_size, "%s", message);
}


b32 neoglobal_problem_evaluate(
    const NeoGlobal_Problem *problem,
    const NeoGlobal_Real *x,
    NeoGlobal_Real *result,
    char *error,
    size_t error_size
)
{
    if (problem == NULL) {
        problem_set_error(
            error,
            error_size,
            "problem is NULL"
        );

        return 0;
    }

    if (problem->evaluate == NULL) {
        problem_set_error(
            error,
            error_size,
            "problem has no evaluator"
        );

        return 0;
    }

    if (x == NULL) {
        problem_set_error(
            error,
            error_size,
            "candidate vector is NULL"
        );

        return 0;
    }

    if (result == NULL) {
        problem_set_error(
            error,
            error_size,
            "result is NULL"
        );

        return 0;
    }

    return problem->evaluate(
        x,
        problem->dimension,
        result,
        problem->evaluator_data,
        error,
        error_size
    );
}


void neoglobal_problem_destroy(NeoGlobal_Problem *problem) {
    if (problem == NULL)
        return;

    if (problem->destroy_evaluator) {
        problem->destroy_evaluator(
            problem->evaluator_data
        );
    }

    free(problem->name);
    free(problem->lower);
    free(problem->upper);

    memset(problem, 0, sizeof(*problem));
}


const char *neoglobal_evaluator_type_name(NeoGlobal_Evaluator_Type type) {
    switch (type) {
        case NEOGLOBAL_EVALUATOR_LUA:
            return "lua";

        case NEOGLOBAL_EVALUATOR_NATIVE:
            return "native";

        case NEOGLOBAL_EVALUATOR_EXTERNAL:
            return "external";

        default:
            return "unknown";
    }
}
