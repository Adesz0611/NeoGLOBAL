return {
    name = "Sphere 2D",

    dimension = 2,

    bounds = {
        lower = -5.0,
        upper =  5.0,
    },

    evaluator = {
        type = "lua",

        evaluate = function(x)
            return x[1] * x[1]
                 + x[2] * x[2]
        end,
    },
}
