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

## COCO BBOB Benchmark

The main `neoglobal` executable continues to load Lua problem files. COCO BBOB
is an optional, separate benchmark executable that links the same NeoGLOBAL
optimizer core to COCO's standard objective functions and logger.

The runner uses the COCO C API, which evaluates `double` vectors, so it is
built in f64 regardless of the `NEOGLOBAL_REAL` setting used for the main
program. The COCO source tree is an optional dependency and is not needed for
normal Lua runs.

1. Initialize the COCO submodule (or set `COCO_DIR` to a separate COCO checkout):

   ```sh
   git submodule update --init third_party/coco
   ```

2. Install the small Python dependency used by COCO's source generator and
   generate its C API files once:

   ```sh
   python3 -m pip install colorama
   python3 third_party/coco/scripts/fabricate \
       --skip-java --skip-matlab --skip-octave --skip-python \
       --skip-postprocessing --skip-unit-tests --skip-integration-tests --skip-rust
   ```

   This creates the amalgamated `coco.c` and `coco.h` under
   `third_party/coco/code-experiments/build/c/` without running COCO's own
   example benchmark.

3. Build the optional NeoGLOBAL and CMA-ES BBOB runners:

   ```sh
   make bbob
   ./build/bbob/neoglobal_bbob 1 2 1 1000
   ./build/bbob/cmaes_bbob 1 2 1 1000
   ```

   The arguments are function index (1–24), dimension, instance, and maximum
   objective evaluations. The example selects Sphere (`f1`), 2 dimensions,
   instance 1, and a budget of 1000 evaluations. CMA-ES is the C implementation
   bundled in COCO's `bbob2009-c-cmaes` example.

4. Run paired trials over a problem matrix. The script builds a unique
   experiment directory, gives both solvers matching problem settings and
   seeds, and writes per-trial data, summary statistics, and one PNG per
   function/dimension/instance:

   ```sh
   python3 -m venv .venv
   .venv/bin/python -m pip install -r requirements-bbob.txt
   .venv/bin/python scripts/compare_bbob.py \
       --functions 1,2,8 --dimensions 2,10 --instances 1,2,3 \
       --runs 5 --budget-per-dimension 500
   ```

   The default evaluation budget is 500 times the dimension. Pass `--budget`
   to use one fixed budget for every case. The script prints the experiment
   directory and a `cocopp` command after it finishes. COCO's postprocessor
   produces standard target-achievement plots and tables from the isolated
   logs in that experiment directory.

   ```sh
   cd exdata/comparisons/<experiment-directory>
   ../../../.venv/bin/python -m cocopp coco_work/exdata/results/
   ```

The benchmark executable does not change the Lua application's entry point or
make COCO a dependency of the ordinary `make` target.

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
