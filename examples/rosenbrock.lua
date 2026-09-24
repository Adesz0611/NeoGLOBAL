return {
    name = "Rosenbrock 4D",

    dimension = 4,

    bounds = {
        lower = -2.0,
        upper =  2.0,
    },

    evaluator = {
        type = "lua",

        evaluate = function(x)
            local sum = 0.0

            for i = 1, #x - 1 do
                local a = x[i + 1] - x[i] * x[i]
                local b = 1.0 - x[i]

                sum = sum
                    + 100.0 * a * a
                    + b * b
            end

            return sum
        end,
    },
}
