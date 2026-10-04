#include <neoglobal/problem.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    lua_State *state;
    int evaluate_ref;
} NeoGlobal_Lua_Evaluator;


static void lua_problem_set_error(
    char *error,
    size_t error_size,
    const char *message
)
{
    if (error == NULL || error_size == 0)
        return;

    snprintf(error, error_size, "%s", message);
}


static char *lua_problem_strdup(CFD_Arena *arena, const char *string) {
    size_t size = strlen(string) + 1;

    char *copy = cfd_arena_alloc(arena, size);

    if (!copy)
        return NULL;

    memcpy(copy, string, size);

    return copy;
}

static b32 lua_problem_read_bounds(
    lua_State *state,
    int bounds_index,
    const char *field_name,
    NeoGlobal_Real *output,
    u32 dimension,
    char *error,
    size_t error_size
)
{
    lua_getfield(state, bounds_index, field_name);

    int type = lua_type(state, -1);

    /*
     * Scalar:
     *
     * lower = -5.12
     */
    if (type == LUA_TNUMBER) {
        lua_Number value = lua_tonumber(state, -1);

        if (!isfinite((double)value)) {
            snprintf(
                error,
                error_size,
                "bounds.%s must be finite",
                field_name
            );

            lua_pop(state, 1);

            return 0;
        }

        NeoGlobal_Real converted = (NeoGlobal_Real)value;

        if (!isfinite((double)converted)) {
            snprintf(
                error,
                error_size,
                "bounds.%s cannot be represented by NeoGlobal_Real",
                field_name
            );

            lua_pop(state, 1);

            return 0;
        }

        for (u32 i = 0; i < dimension; ++i)
            output[i] = converted;

        lua_pop(state, 1);

        return 1;
    }

    /*
     * Array:
     *
     * lower = { -5.0, 0.0, 10.0 }
     */
    if (type == LUA_TTABLE) {
        size_t length = lua_rawlen(state, -1);

        if (length != dimension) {
            snprintf(
                error,
                error_size,
                "bounds.%s contains %zu values, "
                "but dimension is %u",
                field_name,
                length,
                dimension
            );

            lua_pop(state, 1);

            return 0;
        }

        for (u32 i = 0; i < dimension; ++i) {
            lua_rawgeti(
                state,
                -1,
                (lua_Integer)i + 1
            );

            if (lua_type(state, -1) != LUA_TNUMBER) {
                snprintf(
                    error,
                    error_size,
                    "bounds.%s[%u] must be a number",
                    field_name,
                    i + 1
                );

                lua_pop(state, 2);

                return 0;
            }

            lua_Number value = lua_tonumber(state, -1);

            NeoGlobal_Real converted = (NeoGlobal_Real)value;

            if (!isfinite((double)value) || !isfinite((double)converted)) {
                snprintf(
                    error,
                    error_size,
                    "bounds.%s[%u] must be a finite "
                    "NeoGlobal_Real value",
                    field_name,
                    i + 1
                );

                lua_pop(state, 2);

                return 0;
            }

            output[i] = converted;

            lua_pop(state, 1);
        }

        lua_pop(state, 1);

        return 1;
    }

    snprintf(
        error,
        error_size,
        "bounds.%s must be a number or array",
        field_name
    );

    lua_pop(state, 1);

    return 0;
}

static b32 lua_problem_read_unsigned_option(
    lua_State *state,
    int options_index,
    const char *field_name,
    const char *display_name,
    u64 default_value,
    u64 minimum,
    u64 maximum,
    u64 *output,
    char *error,
    size_t error_size
)
{
    lua_getfield(state, options_index, field_name);

    if (lua_isnil(state, -1)) {
        *output = default_value;
        lua_pop(state, 1);
        return true;
    }

    if (!lua_isinteger(state, -1)) {
        snprintf(error, error_size, "%s must be an integer", display_name);
        lua_pop(state, 1);
        return false;
    }

    lua_Integer value = lua_tointeger(state, -1);
    lua_pop(state, 1);

    if (value < 0 || (u64)value < minimum || (u64)value > maximum) {
        snprintf(
            error,
            error_size,
            "%s must be between %llu and %llu",
            display_name,
            (unsigned long long)minimum,
            (unsigned long long)maximum
        );
        return false;
    }

    *output = (u64)value;
    return true;
}

static b32 lua_problem_read_settings(
    lua_State *state,
    int root_index,
    NeoGlobal_Problem *problem,
    char *error,
    size_t error_size
)
{
    problem->random_seed = 42;
    problem->max_evaluations = 100000;
    problem->samples_per_iteration = 1000;
    problem->reduced_samples = 20;
    problem->alpha = 0.5;
    problem->local_search_max_evaluations = 1000;
    problem->local_search_relative_tolerance = 1e-6;

    u64 samples_per_iteration = problem->samples_per_iteration;
    u64 reduced_samples = problem->reduced_samples;

    struct {
        const char *table_name;
        const char *field_name;
        const char *display_name;
        u64 default_value;
        u64 minimum;
        u64 maximum;
        u64 *output;
    } options[] = {
        {
            "random", "seed", "random.seed",
            problem->random_seed, 0, UINT64_MAX, &problem->random_seed
        },
        {
            "stopping", "max_evaluations", "stopping.max_evaluations",
            problem->max_evaluations, 1, UINT64_MAX, &problem->max_evaluations
        },
        {
            "neoglobal", "samples_per_iteration", "neoglobal.samples_per_iteration",
            problem->samples_per_iteration, 1, UINT32_MAX,
            &samples_per_iteration
        },
        {
            "neoglobal", "reduced_samples", "neoglobal.reduced_samples",
            problem->reduced_samples, 1, UINT32_MAX,
            &reduced_samples
        },
        {
            "local_search", "max_evaluations", "local_search.max_evaluations",
            problem->local_search_max_evaluations, 1, UINT64_MAX,
            &problem->local_search_max_evaluations
        },
    };

    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        lua_getfield(state, root_index, options[i].table_name);

        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            continue;
        }

        if (lua_type(state, -1) != LUA_TTABLE) {
            snprintf(error, error_size, "%s must be a table", options[i].table_name);
            lua_pop(state, 1);
            return false;
        }

        int table_index = lua_absindex(state, -1);
        u64 value;
        b32 valid = lua_problem_read_unsigned_option(
            state,
            table_index,
            options[i].field_name,
            options[i].display_name,
            options[i].default_value,
            options[i].minimum,
            options[i].maximum,
            &value,
            error,
            error_size
        );
        lua_pop(state, 1);

        if (!valid)
            return false;

        *options[i].output = value;
    }

    problem->samples_per_iteration = (u32)samples_per_iteration;
    problem->reduced_samples = (u32)reduced_samples;

    if (problem->reduced_samples > problem->samples_per_iteration) {
        lua_problem_set_error(
            error,
            error_size,
            "neoglobal.reduced_samples cannot exceed neoglobal.samples_per_iteration"
        );
        return false;
    }

    lua_getfield(state, root_index, "neoglobal");
    if (lua_type(state, -1) == LUA_TTABLE) {
        lua_getfield(state, -1, "alpha");

        if (!lua_isnil(state, -1)) {
            if (lua_type(state, -1) != LUA_TNUMBER) {
                lua_problem_set_error(error, error_size, "neoglobal.alpha must be a number");
                lua_pop(state, 2);
                return false;
            }

            lua_Number alpha = lua_tonumber(state, -1);
            if (!isfinite((double)alpha) || alpha < 0.0 || alpha > 1.0) {
                lua_problem_set_error(error, error_size, "neoglobal.alpha must be in [0, 1]");
                lua_pop(state, 2);
                return false;
            }

            problem->alpha = (f64)alpha;
        }

        lua_pop(state, 1);
    }
    lua_pop(state, 1);

    lua_getfield(state, root_index, "local_search");
    if (lua_type(state, -1) == LUA_TTABLE) {
        lua_getfield(state, -1, "relative_tolerance");

        if (!lua_isnil(state, -1)) {
            if (lua_type(state, -1) != LUA_TNUMBER) {
                lua_problem_set_error(
                    error,
                    error_size,
                    "local_search.relative_tolerance must be a number"
                );
                lua_pop(state, 2);
                return false;
            }

            lua_Number tolerance = lua_tonumber(state, -1);
            if (!isfinite((double)tolerance) || tolerance <= 0.0 || tolerance >= 1.0) {
                lua_problem_set_error(
                    error,
                    error_size,
                    "local_search.relative_tolerance must be in (0, 1)"
                );
                lua_pop(state, 2);
                return false;
            }

            problem->local_search_relative_tolerance = (f64)tolerance;
        }

        lua_pop(state, 1);
    }
    lua_pop(state, 1);

    return true;
}


static b32 lua_problem_evaluate(
    const NeoGlobal_Real *x,
    u32 dimension,
    NeoGlobal_Real *result,
    void *userdata,
    char *error,
    size_t error_size
)
{
    NeoGlobal_Lua_Evaluator *evaluator = userdata;

    lua_State *state = evaluator->state;

    /*
     * Retrieve evaluate() from the Lua registry.
     */
    lua_rawgeti(state, LUA_REGISTRYINDEX, evaluator->evaluate_ref);

    /*
     * Build:
     *
     * x = { x1, x2, ... }
     */
    lua_newtable(state);

    for (u32 i = 0; i < dimension; ++i) {
        lua_pushnumber(
            state,
            (lua_Number)x[i]
        );

        lua_rawseti(
            state,
            -2,
            (lua_Integer)i + 1
        );
    }

    /*
     * evaluate(x)
     */
    if (lua_pcall(state, 1, 1, 0) != LUA_OK) {
        const char *message = lua_tostring(state, -1);

        snprintf(
            error,
            error_size,
            "Lua evaluator failed: %s",
            message ? message : "unknown error"
        );

        lua_pop(state, 1);

        return 0;
    }

    if (lua_type(state, -1) != LUA_TNUMBER) {
        lua_problem_set_error(
            error,
            error_size,
            "Lua evaluator must return a number"
        );

        lua_pop(state, 1);

        return 0;
    }

    lua_Number lua_result = lua_tonumber(state, -1);

    lua_pop(state, 1);

    NeoGlobal_Real converted = (NeoGlobal_Real)lua_result;

    if (!isfinite((double)lua_result) || !isfinite((double)converted)) {
        lua_problem_set_error(
            error,
            error_size,
            "Lua evaluator returned a non-finite value"
        );

        return 0;
    }

    *result = converted;

    return 1;
}


static void lua_problem_destroy_evaluator(void *userdata) {
    NeoGlobal_Lua_Evaluator *evaluator = userdata;

    if (!evaluator)
        return;

    if (evaluator->state)
        lua_close(evaluator->state);

    free(evaluator);
}


b32 neoglobal_problem_load_lua(
    CFD_Arena *arena,
    const char *path,
    NeoGlobal_Problem *problem,
    char *error,
    size_t error_size
)
{
    if (!arena || !arena->buffer || !path || !problem) {
        lua_problem_set_error(
            error,
            error_size,
            "invalid argument"
        );

        return 0;
    }

    u64 arena_start = arena->offset;
    memset(problem, 0, sizeof(*problem));

    lua_State *state = luaL_newstate();

    if (!state) {
        lua_problem_set_error(
            error,
            error_size,
            "failed to create Lua state"
        );

        return 0;
    }

    luaL_openlibs(state);

    /*
     * Load problem.lua.
     */
    if (luaL_loadfile(state, path) != LUA_OK) {
        snprintf(
            error,
            error_size,
            "failed to load '%s': %s",
            path,
            lua_tostring(state, -1)
        );

        lua_close(state);

        return 0;
    }

    /*
     * Execute problem.lua.
     * We expect one return value.
     */
    if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
        snprintf(
            error,
            error_size,
            "failed to execute '%s': %s",
            path,
            lua_tostring(state, -1)
        );

        lua_close(state);

        return 0;
    }

    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_problem_set_error(
            error,
            error_size,
            "problem file must return a table"
        );

        lua_close(state);

        return 0;
    }

    int root = lua_absindex(state, -1);

    if (!lua_problem_read_settings(state, root, problem, error, error_size))
        goto fail;

    /*
     * name
     */
    lua_getfield(state, root, "name");

    if (lua_isnil(state, -1)) {
        problem->name = lua_problem_strdup(arena, "Unnamed problem");
    }
    else if (lua_type(state, -1) == LUA_TSTRING) {
        problem->name = lua_problem_strdup(arena, lua_tostring(state, -1));
    }
    else {
        lua_problem_set_error(
            error,
            error_size,
            "name must be a string"
        );

        lua_pop(state, 1);

        goto fail;
    }

    lua_pop(state, 1);

    if (!problem->name) {
        lua_problem_set_error(
            error,
            error_size,
            "out of memory"
        );

        goto fail;
    }

    /*
     * dimension
     */
    lua_getfield(state, root, "dimension");

    if (!lua_isinteger(state, -1)) {
        lua_problem_set_error(
            error,
            error_size,
            "dimension must be an integer"
        );

        lua_pop(state, 1);

        goto fail;
    }

    lua_Integer dimension = lua_tointeger(state, -1);

    lua_pop(state, 1);

    if (dimension <= 0 || (lua_Unsigned)dimension > UINT32_MAX) {
        lua_problem_set_error(
            error,
            error_size,
            "dimension is outside the supported range"
        );

        goto fail;
    }

    problem->dimension = (u32)dimension;

    /*
     * Allocate bounds.
     */
    problem->lower = cfd_arena_push_array(arena, NeoGlobal_Real, problem->dimension);
    problem->upper = cfd_arena_push_array(arena, NeoGlobal_Real, problem->dimension);

    if (!problem->lower || !problem->upper) {
        lua_problem_set_error(
            error,
            error_size,
            "out of memory"
        );

        goto fail;
    }

    /*
     * bounds
     */
    lua_getfield(state, root, "bounds");

    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_problem_set_error(
            error,
            error_size,
            "bounds must be a table"
        );

        lua_pop(state, 1);

        goto fail;
    }

    int bounds = lua_absindex(state, -1);

    if (
        !lua_problem_read_bounds(
            state,
            bounds,
            "lower",
            problem->lower,
            problem->dimension,
            error,
            error_size
        )
    ) {
        lua_pop(state, 1);

        goto fail;
    }

    if (
        !lua_problem_read_bounds(
            state,
            bounds,
            "upper",
            problem->upper,
            problem->dimension,
            error,
            error_size
        )
    ) {
        lua_pop(state, 1);

        goto fail;
    }

    lua_pop(state, 1);

    for (u32 i = 0; i < problem->dimension; ++i) {
        if (problem->lower[i] >= problem->upper[i]) {
            snprintf(
                error,
                error_size,
                "invalid bounds for dimension %u: "
                "lower (%g) must be less than upper (%g)",
                i + 1,
                (double)problem->lower[i],
                (double)problem->upper[i]
            );

            goto fail;
        }
    }

    /*
     * evaluator
     */
    lua_getfield(state, root, "evaluator");

    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_problem_set_error(
            error,
            error_size,
            "evaluator must be a table"
        );

        lua_pop(state, 1);

        goto fail;
    }

    int evaluator_table = lua_absindex(state, -1);

    lua_getfield(state, evaluator_table, "type");

    if (lua_type(state, -1) != LUA_TSTRING) {
        lua_problem_set_error(
            error,
            error_size,
            "evaluator.type must be a string"
        );

        lua_pop(state, 2);

        goto fail;
    }

    const char *type = lua_tostring(state, -1);

    if (strcmp(type, "lua") != 0) {
        snprintf(
            error,
            error_size,
            "evaluator type '%s' is not implemented yet",
            type
        );

        lua_pop(state, 2);

        goto fail;
    }

    lua_pop(state, 1);

    /*
     * evaluator.evaluate
     */
    lua_getfield(state, evaluator_table, "evaluate");

    if (lua_type(state, -1) != LUA_TFUNCTION) {
        lua_problem_set_error(
            error,
            error_size,
            "evaluator.evaluate must be a function"
        );

        lua_pop(state, 2);

        goto fail;
    }

    NeoGlobal_Lua_Evaluator *lua_evaluator = calloc(1, sizeof(*lua_evaluator));

    if (!lua_evaluator) {
        lua_problem_set_error(
            error,
            error_size,
            "out of memory"
        );

        lua_pop(state, 2);

        goto fail;
    }

    lua_evaluator->state = state;

    /*
     * Stores the function in the Lua registry
     * and pops it from the stack.
     */
    lua_evaluator->evaluate_ref = luaL_ref(state, LUA_REGISTRYINDEX);

    problem->evaluator_type = NEOGLOBAL_EVALUATOR_LUA;

    problem->evaluate = lua_problem_evaluate;

    problem->destroy_evaluator = lua_problem_destroy_evaluator;

    problem->evaluator_data = lua_evaluator;

    lua_pop(state, 1); /* evaluator */
    lua_pop(state, 1); /* root */

    return 1;


fail:
    memset(problem, 0, sizeof(*problem));
    arena->offset = arena_start;

    lua_close(state);

    return 0;
}
