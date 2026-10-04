#include <neoglobal/clustering.h>
#include <neoglobal/local_search.h>
#include <neoglobal/optimizer.h>
#include <neoglobal/sampling.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static b32 test_uniform_sampling_and_seed(void) {
    NeoGlobal_Real lower[3] = {
        (NeoGlobal_Real)-2.0,
        (NeoGlobal_Real)10.0,
        (NeoGlobal_Real)-0.25,
    };
    NeoGlobal_Real upper[3] = {
        (NeoGlobal_Real)3.0,
        (NeoGlobal_Real)20.0,
        (NeoGlobal_Real)0.75,
    };
    NeoGlobal_Problem problem = {0};
    problem.dimension = 3;
    problem.lower = lower;
    problem.upper = upper;

    CFD_Rng rng_a;
    CFD_Rng rng_b;
    cfd_rng_seed(&rng_a, 987654321);
    cfd_rng_seed(&rng_b, 987654321);

    NeoGlobal_Real a[3];
    NeoGlobal_Real b[3];
    for (u32 sample_index = 0; sample_index < 1000; ++sample_index) {
        TEST_CHECK(neoglobal_sample_uniform(&problem, &rng_a, a));
        TEST_CHECK(neoglobal_sample_uniform(&problem, &rng_b, b));
        TEST_CHECK(memcmp(a, b, sizeof(a)) == 0);

        for (u32 i = 0; i < problem.dimension; ++i)
            TEST_CHECK(a[i] >= lower[i] && a[i] <= upper[i]);
    }

    return true;
}

static b32 test_pool_reduction(void) {
    CFD_Arena arena;
    TEST_CHECK(cfd_arena_init(&arena, MB(1)));

    NeoGlobal_Sample_Pool pool;
    TEST_CHECK(neoglobal_sample_pool_init(&arena, 4, &pool));

    NeoGlobal_Real points[4] = {0, 1, 2, 3};
    const NeoGlobal_Real objectives[4] = {4, 1, 3, 2};
    for (u32 i = 0; i < 4; ++i) {
        NeoGlobal_Sample sample = {&points[i], objectives[i]};
        TEST_CHECK(neoglobal_sample_pool_add(&pool, &sample));
    }

    NeoGlobal_Sample selected[2];
    TEST_CHECK(neoglobal_sample_pool_take_best(&pool, 2, selected, 2));
    TEST_CHECK(selected[0].objective == 1);
    TEST_CHECK(selected[1].objective == 2);
    TEST_CHECK(selected[0].x == &points[1]);
    TEST_CHECK(selected[1].x == &points[3]);
    TEST_CHECK(pool.count == 2);
    TEST_CHECK(pool.items[0].objective == 3);
    TEST_CHECK(pool.items[1].objective == 4);

    cfd_arena_destroy(&arena);
    return true;
}

static b32 test_cluster_assignment(void) {
    CFD_Arena arena;
    TEST_CHECK(cfd_arena_init(&arena, MB(1)));

    NeoGlobal_Real lower[1] = {0};
    NeoGlobal_Real upper[1] = {10};
    NeoGlobal_Real center_x[1] = {2};
    NeoGlobal_Real near_x[1] = {(NeoGlobal_Real)2.5};
    NeoGlobal_Real better_x[1] = {(NeoGlobal_Real)2.25};
    NeoGlobal_Real far_x[1] = {4};

    NeoGlobal_Problem problem = {0};
    problem.dimension = 1;
    problem.lower = lower;
    problem.upper = upper;
    problem.alpha = 0.0; /* Makes d_c = 1 for this small membership test. */

    NeoGlobal_Cluster_Set clusters;
    TEST_CHECK(neoglobal_cluster_set_init(&arena, 4, 16, &clusters));

    NeoGlobal_Sample center = {center_x, 1};
    TEST_CHECK(neoglobal_cluster_set_create(&clusters, &center, NULL));

    NeoGlobal_Sample reduced[3] = {
        {near_x, 2},
        {better_x, (NeoGlobal_Real)0.5},
        {far_x, 3},
    };
    u32 reduced_count = 3;
    char error[256] = {0};
    TEST_CHECK(neoglobal_clusters_assign_reduced(
        &problem, &clusters, reduced, &reduced_count, error, sizeof(error)
    ));

    TEST_CHECK(clusters.items[0].member_count == 2);
    TEST_CHECK(reduced_count == 2);
    TEST_CHECK(reduced[0].x == better_x);
    TEST_CHECK(reduced[1].x == far_x);

    cfd_arena_destroy(&arena);
    return true;
}

static b32 test_coordinate_local_search(void) {
    CFD_Arena arena;
    TEST_CHECK(cfd_arena_init(&arena, MB(1)));

    NeoGlobal_Real lower[1] = {-1};
    NeoGlobal_Real upper[1] = {1};
    NeoGlobal_Real start_x[1] = {(NeoGlobal_Real)0.8};
    NeoGlobal_Problem problem = {0};
    problem.dimension = 1;
    problem.lower = lower;
    problem.upper = upper;

    /* Native test objective: f(x) = x^2. */
    NeoGlobal_Sample start = {start_x, (NeoGlobal_Real)0.64};
    NeoGlobal_Real workspace_x[1];
    f64 workspace_steps[1];
    NeoGlobal_Sample result;
    u64 evaluations_used = 0;
    char error[256] = {0};

    /* Install a small test evaluator through the normal problem interface. */
    extern b32 test_sphere_evaluator(
        const NeoGlobal_Real *, u32, NeoGlobal_Real *, void *, char *, size_t
    );
    problem.evaluate = test_sphere_evaluator;

    TEST_CHECK(neoglobal_local_search_coordinate(
        &arena,
        &problem,
        &start,
        workspace_x,
        workspace_steps,
        200,
        1e-5,
        &result,
        &evaluations_used,
        error,
        sizeof(error)
    ));

    TEST_CHECK(evaluations_used > 0 && evaluations_used <= 200);
    TEST_CHECK(result.objective < (NeoGlobal_Real)1e-8);
    TEST_CHECK(fabs((double)result.x[0]) < 1e-4);

    cfd_arena_destroy(&arena);
    return true;
}

b32 test_sphere_evaluator(
    const NeoGlobal_Real *x,
    u32 dimension,
    NeoGlobal_Real *result,
    void *userdata,
    char *error,
    size_t error_size
)
{
    (void)userdata;
    (void)error;
    (void)error_size;
    NeoGlobal_Real sum = 0;
    for (u32 i = 0; i < dimension; ++i)
        sum += x[i] * x[i];
    *result = sum;
    return true;
}

static b32 test_lua_to_optimizer(void) {
    CFD_Arena arena;
    TEST_CHECK(cfd_arena_init(&arena, MB(16)));

    NeoGlobal_Problem *problem = cfd_arena_push_type(&arena, NeoGlobal_Problem);
    TEST_CHECK(problem != NULL);

    char error[512] = {0};
    TEST_CHECK(neoglobal_problem_load_lua(
        &arena,
        "tests/problems/sphere_small.lua",
        problem,
        error,
        sizeof(error)
    ));
    TEST_CHECK(problem->dimension == 2);
    TEST_CHECK(problem->max_evaluations == 400);
    TEST_CHECK(problem->local_search_max_evaluations == 80);

    NeoGlobal_Result result;
    TEST_CHECK(neoglobal_optimize(&arena, problem, &result, error, sizeof(error)));
    TEST_CHECK(result.evaluations == problem->max_evaluations);
    TEST_CHECK(result.iterations > 0);
    TEST_CHECK(result.local_searches > 0);
    TEST_CHECK(result.clusters > 0);
    TEST_CHECK(result.best.objective < (NeoGlobal_Real)1e-3);

    neoglobal_problem_destroy(problem);
    cfd_arena_destroy(&arena);
    return true;
}

int main(void) {
    struct {
        const char *name;
        b32 (*run)(void);
    } cases[] = {
        {"uniform sampling and seed", test_uniform_sampling_and_seed},
        {"sample-pool reduction", test_pool_reduction},
        {"cluster assignment", test_cluster_assignment},
        {"coordinate local search", test_coordinate_local_search},
        {"Lua-to-optimizer integration", test_lua_to_optimizer},
    };

    for (u32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!cases[i].run())
            return 1;
        printf("PASS %s\n", cases[i].name);
    }

    return 0;
}
