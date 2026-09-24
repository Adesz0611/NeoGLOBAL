# NeoGLOBAL Problem File Specification

NeoGLOBAL optimization problems are defined using Lua files.

A problem file describes the **search space**, the **evaluation backend**, and optional execution settings required to solve an optimization problem.

The Lua file must return a single table.

---

## Minimal Example

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

A problem file may be passed to NeoGLOBAL as:

```bash
neoglobal problem.lua
```

---

## Overview

A NeoGLOBAL problem file may contain the following top-level fields:

| Field | Type | Required | Description |
|---|---|---:|---|
| `name` | string | No | Human-readable problem name |
| `dimension` | positive integer | Yes | Number of decision variables |
| `bounds` | table | Yes | Lower and upper bounds of the search space |
| `evaluator` | table | Yes | Describes how candidate solutions are evaluated |
| `stopping` | table | No | Termination conditions |
| `random` | table | No | Random number generator configuration |
| `neoglobal` | table | No | NeoGLOBAL-specific algorithm configuration |

The problem file defines **what should be optimized**.

The NeoGLOBAL core determines **how the optimization is performed**.

---

# Problem Metadata

## `name`

```lua
name = "CFD Airfoil Optimization"
```

Human-readable name of the optimization problem.

**Type:** `string`  
**Required:** No

The value does not affect the mathematical behavior of the optimizer.

It may be used for:

- log messages,
- benchmark reports,
- result files,
- experiment identification.

Example:

```lua
name = "Rosenbrock 50D"
```

---

# Search Space

## `dimension`

```lua
dimension = 10
```

Defines the number of decision variables.

**Type:** positive integer  
**Required:** Yes

For:

```lua
dimension = 3
```

NeoGLOBAL searches over vectors of the form:

\[
x = (x_1, x_2, x_3)
\]

Lua evaluators access these values using:

```lua
x[1]
x[2]
x[3]
```

---

## `bounds`

The `bounds` table defines the allowed search interval for every decision variable.

```lua
bounds = {
    lower = ...,
    upper = ...,
}
```

For each dimension \(i\):

\[
lower_i \le x_i \le upper_i
\]

Both `bounds.lower` and `bounds.upper` are required.

NeoGLOBAL supports two forms.

---

## Uniform Bounds

If every dimension uses the same interval, scalar values may be used:

```lua
dimension = 100,

bounds = {
    lower = -5.12,
    upper =  5.12,
},
```

This means:

\[
-5.12 \le x_i \le 5.12
\]

for every dimension.

This form is recommended when all dimensions share identical bounds.

---

## Per-Dimension Bounds

Different bounds may be provided for every dimension:

```lua
dimension = 4,

bounds = {
    lower = {
        0.1,
        -5.0,
        0.01,
        100000.0,
    },

    upper = {
        0.5,
        15.0,
        0.10,
        500000.0,
    },
},
```

This defines:

\[
0.1 \le x_1 \le 0.5
\]

\[
-5.0 \le x_2 \le 15.0
\]

\[
0.01 \le x_3 \le 0.10
\]

\[
100000 \le x_4 \le 500000
\]

When arrays are used, their lengths must match `dimension`.

---

# Evaluators

The `evaluator` table describes how NeoGLOBAL maps a candidate vector

\[
x = (x_1, x_2, \ldots, x_n)
\]

to a numeric objective value

\[
f(x)
\]

NeoGLOBAL is designed so that the optimization core remains independent of the software used to evaluate candidates.

The general data flow is:

```text
candidate vector
      |
      v
+-------------+
|  evaluator  |
+-------------+
      |
      v
objective value
      |
      v
   NeoGLOBAL
```

The first versions of NeoGLOBAL may support several evaluator backends:

- Lua functions,
- native shared libraries,
- external programs and simulation software.

---

## Lua Evaluator

Lua evaluators are intended primarily for:

- benchmark functions,
- tests,
- examples,
- prototypes,
- inexpensive mathematical objectives.

Example:

```lua
evaluator = {
    type = "lua",

    evaluate = function(x)
        return x[1] * x[1] + x[2] * x[2]
    end,
}
```

### Fields

| Field | Type | Required | Description |
|---|---|---:|---|
| `type` | `"lua"` | Yes | Selects the Lua evaluator backend |
| `evaluate` | function | Yes | Computes the objective value |

The evaluator function receives the candidate vector `x` and must return one finite numeric value.

Example:

```lua
evaluator = {
    type = "lua",

    evaluate = function(x)
        local sum = 0.0

        for i = 1, #x do
            sum = sum + x[i] * x[i]
        end

        return sum
    end,
}
```

---

## Native Evaluator

Native evaluators allow NeoGLOBAL to call a function from a compiled shared library.

This is intended for:

- high-performance objective functions,
- existing C or C-compatible scientific software,
- simulation libraries,
- workloads where Lua call overhead should be avoided.

Example configuration:

```lua
evaluator = {
    type = "native",
    library = "./libproblem.so",
    symbol = "evaluate",
}
```

### Fields

| Field | Type | Required | Description |
|---|---|---:|---|
| `type` | `"native"` | Yes | Selects the native evaluator backend |
| `library` | string | Yes | Path to the shared library |
| `symbol` | string | Yes | Name of the evaluator function |

A possible native API is:

```c
double evaluate(
    const double *x,
    size_t dimension,
    void *userdata
);
```

The exact native ABI will be documented separately once finalized.

---

## External Evaluator

External evaluators allow NeoGLOBAL to use independent software to evaluate candidate solutions.

Typical use cases include:

- CFD solvers,
- FEM software,
- physics simulations,
- engineering optimization tools,
- custom command-line applications,
- existing scientific workflows.

Example:

```lua
evaluator = {
    type = "external",
    command = "./run_simulation",
}
```

Conceptually:

```text
NeoGLOBAL
    |
    | candidate x
    v
external program
    |
    | objective value
    v
NeoGLOBAL
```

### Fields

| Field | Type | Required | Description |
|---|---|---:|---|
| `type` | `"external"` | Yes | Selects the external evaluator backend |
| `command` | string | Yes | Program or command used to evaluate candidates |

The exact communication protocol between NeoGLOBAL and external programs will be documented separately.

Possible implementations may include:

- command-line arguments,
- standard input/output,
- temporary files,
- pipes,
- sockets,
- or a dedicated worker protocol.

NeoGLOBAL should avoid coupling the optimizer core to a specific external simulation package.

---

# Objective Direction

NeoGLOBAL initially assumes that all problems are **minimization problems**.

Therefore:

```text
smaller objective value = better solution
```

For a maximization problem

\[
\max f(x)
\]

the evaluator may return:

\[
-f(x)
\]

instead.

For example:

```lua
evaluator = {
    type = "lua",

    evaluate = function(x)
        return -original_function(x)
    end,
}
```

A future version may introduce an explicit field such as:

```lua
direction = "minimize"
```

or:

```lua
direction = "maximize"
```

---

# Stopping Conditions

## `stopping`

The optional `stopping` table configures termination conditions.

Example:

```lua
stopping = {
    max_evaluations = 100000,
    tolerance = 1e-8,
}
```

---

## `stopping.max_evaluations`

```lua
max_evaluations = 100000
```

Maximum number of objective function evaluations.

**Type:** positive integer  
**Required:** No

Every evaluated candidate increments the function evaluation counter.

This quantity is commonly referred to as:

```text
NFE = Number of Function Evaluations
```

Limiting the NFE is particularly important when a single evaluation involves an expensive external simulation.

---

## `stopping.tolerance`

```lua
tolerance = 1e-8
```

Numerical tolerance used by NeoGLOBAL.

**Type:** positive number  
**Required:** No

The precise semantics of this value depend on the corresponding algorithmic component and will be documented together with the implementation.

It may be used when determining whether two values or solutions should be considered numerically equivalent.

---

# Random Number Generation

## `random`

The optional `random` table controls pseudo-random number generation.

```lua
random = {
    seed = 42,
}
```

---

## `random.seed`

```lua
seed = 42
```

Initial seed of NeoGLOBAL's pseudo-random number generator.

**Type:** non-negative integer  
**Required:** No

Explicit seeds are strongly recommended for:

- benchmarks,
- regression tests,
- research experiments,
- reproducibility.

Example:

```lua
random = {
    seed = 123456789,
}
```

Parallel or distributed execution may introduce additional sources of non-determinism depending on the execution strategy.

---

# NeoGLOBAL Algorithm Settings

## `neoglobal`

The optional `neoglobal` table is reserved for settings specific to the NeoGLOBAL algorithm.

```lua
neoglobal = {
    ...
}
```

These settings define **how the optimizer behaves**, rather than defining the mathematical optimization problem itself.

Possible future parameters may include settings related to:

- sample generation,
- reduction,
- clustering,
- local search,
- worker behavior,
- pooling,
- parallel execution,
- distributed communication.

For example, a future version might support:

```lua
neoglobal = {
    samples_per_iteration = 1000,
    reduced_samples = 20,
}
```

The exact interface is intentionally not finalized yet.

Algorithm-specific parameters should only be added once their semantics are well-defined and implemented.

---

# Complete Lua Benchmark Example

The following example defines the 10-dimensional Rastrigin function.

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

    stopping = {
        max_evaluations = 100000,
        tolerance = 1e-8,
    },

    random = {
        seed = 42,
    },
}
```

---

# External Simulation Example

A real-world problem may define only the search space and external evaluator.

```lua
return {
    name = "CFD Airfoil Optimization",

    dimension = 4,

    bounds = {
        lower = {
            0.1,
            -5.0,
            0.01,
            100000.0,
        },

        upper = {
            0.5,
            15.0,
            0.10,
            500000.0,
        },
    },

    evaluator = {
        type = "external",
        command = "./run_cfd",
    },

    stopping = {
        max_evaluations = 10000,
    },

    random = {
        seed = 42,
    },
}
```

NeoGLOBAL may evaluate candidate vectors using the external program while keeping sampling, reduction, clustering, local search coordination, and parallel execution inside the NeoGLOBAL core.

---

# Native Library Example

```lua
return {
    name = "Native Simulation Example",

    dimension = 8,

    bounds = {
        lower = -1.0,
        upper =  1.0,
    },

    evaluator = {
        type = "native",
        library = "./libsimulation.so",
        symbol = "evaluate",
    },

    stopping = {
        max_evaluations = 50000,
    },
}
```

---

# Validation Rules

NeoGLOBAL should validate the problem definition before starting optimization.

## Dimension

The following must hold:

```text
dimension > 0
```

`dimension` must be an integer.

---

## Bounds

For every dimension:

```text
lower < upper
```

If arrays are used:

```text
#lower == dimension
#upper == dimension
```

All bound values must be finite numbers.

NeoGLOBAL should reject inconsistent mixed forms unless explicitly supported.

For example, this should be rejected:

```lua
bounds = {
    lower = -5.0,
    upper = { 5.0, 5.0, 5.0 },
}
```

---

## Evaluator

The `evaluator` field must be a table with a supported `type`.

For a Lua evaluator:

```text
evaluator.type == "lua"
evaluator.evaluate is a function
```

For a native evaluator:

```text
evaluator.type == "native"
evaluator.library is a string
evaluator.symbol is a string
```

For an external evaluator:

```text
evaluator.type == "external"
evaluator.command is a string
```

An evaluator must return exactly one finite numeric objective value for every valid candidate.

`NaN`, positive infinity, and negative infinity should be treated as evaluator errors unless explicitly supported by future versions.

---

## Stopping Conditions

If present:

```text
max_evaluations > 0
```

and:

```text
tolerance > 0
```

---

# Error Messages

Configuration errors should produce clear and actionable diagnostics.

Examples:

```text
NeoGLOBAL configuration error:
bounds.lower contains 3 values, but dimension is 4.
```

```text
NeoGLOBAL configuration error:
invalid bounds for dimension 2:
lower = 5.0
upper = -5.0
```

```text
NeoGLOBAL configuration error:
unsupported evaluator type: "python"
```

```text
NeoGLOBAL evaluator error:
Lua evaluator returned NaN.
```

Whenever possible, invalid configurations should be rejected before optimization begins.

---

# Design Principles

NeoGLOBAL problem files should be:

- simple,
- readable,
- reproducible,
- easy to version-control,
- independent of the optimization core,
- suitable for both benchmarks and real-world simulations.

The Lua configuration layer should describe the problem without embedding NeoGLOBAL's internal implementation details.

The optimization core should remain independent of Lua and interact with evaluators through a common internal interface.

Conceptually:

```text
                    problem.lua
                        |
                        v
                +---------------+
                | Lua frontend  |
                +-------+-------+
                        |
                        v
                +---------------+
                | NeoGLOBAL Core|
                +-------+-------+
                        |
           +------------+------------+
           |            |            |
           v            v            v
        Lua         Native       External
      evaluator     library      simulator
                                   CFD/FEM
           |            |            |
           +------------+------------+
                        |
                        v
                 objective value
```

This separation allows future frontends and evaluator implementations to be added without redesigning the optimization algorithm.

---

# Versioning

The problem file format should be treated as a public interface.

Once NeoGLOBAL reaches a stable release, breaking changes to the problem-file schema should be avoided or introduced through explicit format versioning.

A future schema may therefore include:

```lua
format_version = 1
```

This field is not required by the initial development version.

---

# Current Status

The NeoGLOBAL Lua problem-file format is currently under development.

Fields documented as future or planned functionality may change before the first stable release.

The stable specification should always match the behavior implemented by NeoGLOBAL.

