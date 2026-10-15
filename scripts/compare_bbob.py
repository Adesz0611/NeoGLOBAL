#!/usr/bin/env python3
"""Run paired NeoGLOBAL and COCO C CMA-ES trials on a BBOB problem matrix."""

import argparse
import csv
import itertools
import os
import re
import shlex
import statistics
import subprocess
import sys
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BINARIES = {
    "NeoGLOBAL": ROOT / "build/bbob/neoglobal_bbob",
    "CMA-ES": ROOT / "build/bbob/cmaes_bbob",
}
MAX_CMAES_SEED = 2_000_000_000
TRIAL_FIELDS = [
    "solver",
    "function",
    "dimension",
    "instance",
    "budget",
    "seed",
    "evaluations",
    "best_f",
    "elapsed_seconds",
]


def parse_int_list(value: str) -> list[int]:
    try:
        values = [int(part.strip()) for part in value.split(",")]
    except ValueError as exc:
        raise argparse.ArgumentTypeError("expected comma-separated integers") from exc
    if not values or any(value <= 0 for value in values):
        raise argparse.ArgumentTypeError("all values must be positive integers")
    if len(values) != len(set(values)):
        raise argparse.ArgumentTypeError("duplicate values are not allowed")
    return values


def parse_output(stdout: str) -> tuple[int, float, float]:
    evaluations = re.findall(r"^\s*Evaluations:\s*(\d+)\s*$", stdout, re.MULTILINE)
    best = re.search(r"^\s*Best f\(x\):\s*([^\s]+)", stdout, re.MULTILINE)
    elapsed = re.search(r"^\s*Elapsed:\s*([\d.eE+-]+)\s+seconds", stdout, re.MULTILINE)
    if not evaluations or best is None or elapsed is None:
        raise ValueError("could not parse solver output:\n" + stdout)
    return int(evaluations[-1]), float(best.group(1)), float(elapsed.group(1))


def run_solver(
    name: str,
    algorithm_name: str,
    function: int,
    dimension: int,
    instance: int,
    budget: int,
    seed: int,
    working_directory: Path,
) -> dict[str, object]:
    command = [
        str(BINARIES[name]),
        str(function),
        str(dimension),
        str(instance),
        str(budget),
        str(seed),
    ]
    if name == "NeoGLOBAL":
        command.append(algorithm_name)
    completed = subprocess.run(command, cwd=working_directory, text=True, capture_output=True)
    if completed.returncode != 0:
        raise RuntimeError(
            f"{name} exited with {completed.returncode} for f{function}, "
            f"{dimension}D, instance {instance}, seed {seed}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )

    evaluations, best_f, elapsed = parse_output(completed.stdout)
    result_name = algorithm_name if name == "NeoGLOBAL" else name
    print(
        f"{result_name:20s} f={function:02d} d={dimension:2d} i={instance:2d} "
        f"seed={seed:4d} evals={evaluations:7d}/{budget:<7d} "
        f"best_f={best_f:.12g} elapsed={elapsed:.6f}s"
    )
    return {
        "solver": algorithm_name if name == "NeoGLOBAL" else name,
        "function": function,
        "dimension": dimension,
        "instance": instance,
        "budget": budget,
        "seed": seed,
        "evaluations": evaluations,
        "best_f": best_f,
        "elapsed_seconds": elapsed,
    }


def append_trial(path: Path, row: dict[str, object]) -> None:
    with path.open("a", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=TRIAL_FIELDS)
        writer.writerow(row)


def summarize(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    keys = ("function", "dimension", "instance", "budget", "solver")
    groups: dict[tuple[object, ...], list[dict[str, object]]] = {}
    for row in rows:
        key = tuple(row[field] for field in keys)
        groups.setdefault(key, []).append(row)

    summaries: list[dict[str, object]] = []
    for key, group in sorted(groups.items()):
        times = [float(row["elapsed_seconds"]) for row in group]
        evaluations = [int(row["evaluations"]) for row in group]
        objectives = [float(row["best_f"]) for row in group]
        summary = dict(zip(keys, key))
        summary.update(
            {
                "runs": len(group),
                "mean_evaluations": statistics.mean(evaluations),
                "median_evaluations": statistics.median(evaluations),
                "mean_elapsed_seconds": statistics.mean(times),
                "median_elapsed_seconds": statistics.median(times),
                "best_f": min(objectives),
                "median_best_f": statistics.median(objectives),
                "worst_best_f": max(objectives),
            }
        )
        summaries.append(summary)
    return summaries


def write_summary(path: Path, rows: list[dict[str, object]]) -> None:
    summaries = summarize(rows)
    fields = [
        "function",
        "dimension",
        "instance",
        "budget",
        "solver",
        "runs",
        "mean_evaluations",
        "median_evaluations",
        "mean_elapsed_seconds",
        "median_elapsed_seconds",
        "best_f",
        "median_best_f",
        "worst_best_f",
    ]
    with path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summaries)


def plot_results(
    rows: list[dict[str, object]],
    title: str,
    output: Path,
    solver_names: list[str],
) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise RuntimeError("install matplotlib to generate plots: python3 -m pip install matplotlib") from exc

    names = solver_names
    times = [
        [float(row["elapsed_seconds"]) for row in rows if row["solver"] == name]
        for name in names
    ]
    objectives = [
        [float(row["best_f"]) for row in rows if row["solver"] == name]
        for name in names
    ]

    figure, axes = plt.subplots(1, 2, figsize=(10, 4.5))
    axes[0].boxplot(times, showmeans=True)
    axes[0].set_xticks(range(1, len(names) + 1), names)
    axes[0].set_title("Wall-clock time")
    axes[0].set_ylabel("Seconds (lower is better)")
    axes[1].boxplot(objectives, showmeans=True)
    axes[1].set_xticks(range(1, len(names) + 1), names)
    axes[1].set_title("Best objective value")
    axes[1].set_ylabel("f(x) (lower is better)")
    figure.suptitle(title)
    figure.tight_layout()
    figure.savefig(output, dpi=160)
    plt.close(figure)


def create_experiment_directory(base: Path, requested_name: str | None) -> Path:
    base.mkdir(parents=True, exist_ok=True)
    if requested_name:
        if Path(requested_name).name != requested_name or requested_name in (".", ".."):
            raise ValueError("experiment name must be a single directory name")
        experiment = base / requested_name
    else:
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        experiment = base / f"bbob-{timestamp}-{os.getpid()}"
    experiment.mkdir(exist_ok=False)
    return experiment


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--functions", "--function", type=parse_int_list, default=[1],
                        help="comma-separated BBOB function indices (1-24); --function remains an alias")
    parser.add_argument("--dimensions", "--dimension", type=parse_int_list, default=[2],
                        help="comma-separated problem dimensions")
    parser.add_argument("--instances", "--instance", type=parse_int_list, default=[1],
                        help="comma-separated COCO instance indices")
    parser.add_argument("--budget", type=int, default=None,
                        help="fixed maximum evaluations per run; overrides --budget-per-dimension")
    parser.add_argument("--budget-per-dimension", type=int, default=500,
                        help="evaluation budget per dimension (default: 500; 2D gives 1000)")
    parser.add_argument("--runs", type=int, default=10, help="paired runs per function/dimension/instance")
    parser.add_argument("--seed", type=int, default=1, help="first algorithm seed; seeds increase by one per run")
    parser.add_argument(
        "--neoglobal-name",
        default="NeoGLOBAL-baseline",
        help="versioned COCO/CSV label for NeoGLOBAL (default: NeoGLOBAL-baseline)",
    )
    parser.add_argument(
        "--include-coco-results",
        type=Path,
        help="include an existing COCO results directory in the post-processing report",
    )
    parser.add_argument(
        "--no-cmaes",
        action="store_true",
        help="run only NeoGLOBAL; use with --include-coco-results to compare versions without duplicating CMA-ES",
    )
    parser.add_argument("--no-build", action="store_true", help="use already-built solver executables")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "exdata/comparisons",
                        help="base directory; each invocation creates a new experiment folder")
    parser.add_argument("--name", help="optional unique experiment folder name")
    args = parser.parse_args()

    if any(function > 24 for function in args.functions):
        parser.error("BBOB function indices must be between 1 and 24")
    if args.budget is not None and args.budget < 1:
        parser.error("budget must be positive")
    if args.budget_per_dimension < 1 or args.runs < 1 or args.seed < 1:
        parser.error("budget-per-dimension, runs, and seed must be positive")
    if args.seed + args.runs - 1 > MAX_CMAES_SEED:
        parser.error("seed range exceeds the limit supported by the bundled CMA-ES")
    if (
        re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,63}", args.neoglobal_name) is None
        or args.neoglobal_name == "CMA-ES"
    ):
        parser.error("--neoglobal-name must be a unique 1-64 character label using letters, digits, '.', '_' or '-'")
    if args.include_coco_results is not None:
        args.include_coco_results = args.include_coco_results.resolve()
        if not args.include_coco_results.is_dir():
            parser.error(f"COCO results directory not found: {args.include_coco_results}")

    if not args.no_build:
        subprocess.run(["make", "bbob", "-j2"], cwd=ROOT, check=True)
    for path in BINARIES.values():
        if not path.is_file():
            parser.error(f"solver executable not found: {path}")

    try:
        experiment_dir = create_experiment_directory(args.output_dir.resolve(), args.name)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    working_directory = experiment_dir / "coco_work"
    working_directory.mkdir()
    plot_dir = experiment_dir / "plots"
    plot_dir.mkdir()
    trials_path = experiment_dir / "trials.csv"
    with trials_path.open("w", newline="", encoding="utf-8") as output_file:
        csv.DictWriter(output_file, fieldnames=TRIAL_FIELDS).writeheader()
    rows: list[dict[str, object]] = []
    solver_keys = ["NeoGLOBAL"] if args.no_cmaes else list(BINARIES)
    solver_names = [args.neoglobal_name]
    if not args.no_cmaes:
        solver_names.append("CMA-ES")

    cases = list(itertools.product(args.functions, args.dimensions, args.instances))
    print(f"Experiment: {experiment_dir}")
    print(
        f"Cases: {len(cases)} × {args.runs} runs × {len(solver_keys)} solvers "
        f"= {len(cases) * args.runs * len(solver_keys)} solver runs"
    )
    print(f"Budget: {args.budget} fixed" if args.budget is not None else
          f"Budget: {args.budget_per_dimension} × dimension")

    for function, dimension, instance in cases:
        budget = args.budget if args.budget is not None else args.budget_per_dimension * dimension
        case_rows: list[dict[str, object]] = []
        for run_index in range(args.runs):
            seed = args.seed + run_index
            for name in solver_keys:
                row = run_solver(
                    name,
                    args.neoglobal_name,
                    function,
                    dimension,
                    instance,
                    budget,
                    seed,
                    working_directory,
                )
                rows.append(row)
                case_rows.append(row)
                append_trial(trials_path, row)

        stem = f"bbob-f{function:02d}-d{dimension}-i{instance}-b{budget}"
        plot_path = plot_dir / f"{stem}.png"
        title = f"BBOB f{function} · {dimension}D · instance {instance} · budget {budget}"
        plot_results(case_rows, title, plot_path, solver_names)
        print(f"Plot: {plot_path}")

    summary_path = experiment_dir / "summary.csv"
    write_summary(summary_path, rows)
    coco_results = working_directory / "exdata/results"
    print(f"Trials CSV: {trials_path}")
    print(f"Summary CSV: {summary_path}")
    print(f"COCO logs: {coco_results}")
    coco_input_paths = ["coco_work/exdata/results/"]
    if args.include_coco_results is not None:
        coco_input_paths.append(str(args.include_coco_results))
    postprocess_command = (
        f"cd {shlex.quote(str(experiment_dir))} && "
        f"{shlex.quote(sys.executable)} -m cocopp "
        + " ".join(shlex.quote(path) for path in coco_input_paths)
    )
    print(f"COCO postprocess: {postprocess_command}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"BBOB comparison failed: {error}", file=sys.stderr)
        raise SystemExit(1)
