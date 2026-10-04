return {
    name = "Sphere test",
    dimension = 2,

    bounds = {
        lower = -5.0,
        upper = 5.0,
    },

    random = {
        seed = 12345,
    },

    stopping = {
        max_evaluations = 400,
    },

    neoglobal = {
        samples_per_iteration = 20,
        reduced_samples = 5,
        alpha = 0.5,
    },

    local_search = {
        max_evaluations = 80,
        relative_tolerance = 1e-4,
    },

    evaluator = {
        type = "lua",
        evaluate = function(x)
            return x[1] * x[1] + x[2] * x[2]
        end,
    },
}
