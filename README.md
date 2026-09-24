# NeoGLOBAL

**NeoGLOBAL** is a high-performance C implementation and extension of the GLOBAL global optimization algorithm, designed with parallel and distributed execution in mind.

> **Status:** Early development / research project.

## Goals

NeoGLOBAL aims to provide:

* a clean implementation of the GLOBAL optimization algorithm in C,
* support for ParallelGLOBAL-style worker-based optimization,
* efficient shared-memory parallelization,
* distributed execution using MPI,
* reproducible benchmarking,
* Lua-based problem definitions,
* a modular architecture for custom objective functions and local optimizers.

## Problem Definition

Optimization problems are described using Lua files.

The Lua configuration defines the search space and specifies how NeoGLOBAL should evaluate candidate solutions.

For simple benchmarks, the objective function can be implemented directly in Lua:

```lua
return {
    name = "Rastrigin 10D",

    dimension = 10,

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
```

For real-world optimization problems, NeoGLOBAL is intended to support external evaluators such as CFD, FEM, or other simulation software:

```lua
evaluator = {
    type = "external",
    command = "./run_simulation",
}
```

Native evaluators implemented in compiled libraries may also be supported:

```lua
evaluator = {
    type = "native",
    library = "./libproblem.so",
    symbol = "evaluate",
}
```

This allows NeoGLOBAL to remain independent of the software used to compute the objective value.

See [`docs/problem-files.md`](docs/problem-files.md) for the full problem file specification.

## Planned Architecture

```text
                problem.lua
                    |
                    v
            +---------------+
            |   NeoGLOBAL   |
            |      Core     |
            +-------+-------+
                    |
          +---------+---------+
          |         |         |
          v         v         v
        Lua       Native    External
      evaluator   library   simulator
                             CFD / FEM
          |         |         |
          +---------+---------+
                    |
                    v
             objective value
```

The optimization core is designed to remain independent of the evaluator implementation, allowing NeoGLOBAL to optimize anything that can map a candidate vector to a numeric objective value.

## Roadmap

* [ ] Sequential GLOBAL implementation (Baseline)

## Motivation

GLOBAL is a multistart global optimization method that combines random sampling, reduction, clustering, and local search.

NeoGLOBAL aims to revisit this approach using a modern C/HPC-oriented implementation and explore efficient parallel and distributed variants.

## License

License to be determined.

