#ifndef NEOGLOBAL_PROBLEM_H
#define NEOGLOBAL_PROBLEM_H

#include <stddef.h>
#include <cfd_core.h>

#ifdef NEOGLOBAL_USE_F32
typedef f32 NeoGlobal_Real;
#else
typedef f64 NeoGlobal_Real;
#endif

#define NEOGLOBAL_REAL_CAST(x) ((NeoGlobal_Real)(x))

typedef enum {
    NEOGLOBAL_EVALUATOR_LUA,
    NEOGLOBAL_EVALUATOR_NATIVE,
    NEOGLOBAL_EVALUATOR_EXTERNAL,
} NeoGlobal_Evaluator_Type;

typedef b32 NeoGlobal_Evaluate_Fn(
    const NeoGlobal_Real *x,
    u32 dimension,
    NeoGlobal_Real *result,
    void *userdata,
    char *error,
    size_t error_size
);

typedef void NeoGlobal_Evaluator_Destroy_Fn(
    void *userdata
);

typedef struct {
    char *name;

    u32 dimension;

    NeoGlobal_Real *lower;
    NeoGlobal_Real *upper;

    NeoGlobal_Evaluator_Type evaluator_type;

    NeoGlobal_Evaluate_Fn *evaluate;
    NeoGlobal_Evaluator_Destroy_Fn *destroy_evaluator;

    void *evaluator_data;
} NeoGlobal_Problem;

b32 neoglobal_problem_evaluate(
    const NeoGlobal_Problem *problem,
    const NeoGlobal_Real *x,
    NeoGlobal_Real *result,
    char *error,
    size_t error_size
);

/* Releases evaluator resources; arena storage remains caller-owned. */
void neoglobal_problem_destroy(
    NeoGlobal_Problem *problem
);

const char *neoglobal_evaluator_type_name(
    NeoGlobal_Evaluator_Type type
);

/* Problem-owned strings and bounds are allocated from arena. */
b32 neoglobal_problem_load_lua(
    CFD_Arena *arena,
    const char *path,
    NeoGlobal_Problem *problem,
    char *error,
    size_t error_size
);

#endif
