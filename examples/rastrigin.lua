return {
    name = "Rastrigin 2D",

    dimension = 2,

    bounds = {
        lower = -5.12,
        upper =  5.12,
    },

    evaluator = {
        type = "lua",

        evaluate = function(x)
            local sum = 10.0 * #x

            for i = 1, #x do
                sum = sum
                    + x[i] * x[i]
                    - 10.0 * math.cos(2.0 * math.pi * x[i])
            end

            return sum
        end,
    },
}
