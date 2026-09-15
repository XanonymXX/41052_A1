#!/usr/bin/env python3
"""Validate benchmark output, compute robust summaries, and create report figures."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import pandas as pd  # noqa: E402


REQUIRED_COLUMNS = {
    "scenario",
    "distribution",
    "implementation",
    "n",
    "trial",
    "seed",
    "operations",
    "elapsed_ns",
    "ns_per_op",
    "height",
    "estimated_bytes",
    "checksum",
}
IMPLEMENTATIONS = {"red_black_tree", "ordinary_bst", "sorted_vector"}
IMPLEMENTATION_ORDER = ["red_black_tree", "ordinary_bst", "sorted_vector"]
LABELS = {
    "red_black_tree": "Red-black tree",
    "ordinary_bst": "Ordinary BST",
    "sorted_vector": "Sorted vector",
}
COLORS = {
    "red_black_tree": "#b2182b",
    "ordinary_bst": "#2166ac",
    "sorted_vector": "#1b7837",
}
MARKERS = {"red_black_tree": "o", "ordinary_bst": "s", "sorted_vector": "^"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path("results/raw.csv"))
    parser.add_argument("--summary", type=Path, default=Path("results/summary.csv"))
    parser.add_argument("--figures-dir", type=Path, default=Path("results/figures"))
    parser.add_argument("--metadata", type=Path, default=Path("results/metadata.json"))
    parser.add_argument("--expected-repetitions", type=int)
    parser.add_argument("--expected-sizes", help="comma-separated list, for example 250,500,1000")
    parser.add_argument("--compiler", default="c++")
    return parser.parse_args()


def validate_raw(
    frame: pd.DataFrame,
    expected_repetitions: int | None,
    expected_sizes: set[int] | None = None,
) -> None:
    missing_columns = REQUIRED_COLUMNS - set(frame.columns)
    if missing_columns:
        raise ValueError(f"missing CSV columns: {sorted(missing_columns)}")
    if frame.empty:
        raise ValueError("benchmark CSV contains no rows")
    if set(frame["implementation"]) != IMPLEMENTATIONS:
        raise ValueError("benchmark CSV does not contain exactly the three expected implementations")

    numeric_required = ["n", "trial", "seed", "operations", "elapsed_ns", "ns_per_op", "estimated_bytes"]
    if frame[numeric_required].isna().any().any():
        raise ValueError("required numeric benchmark values contain NaN")
    for column in ["operations", "elapsed_ns", "ns_per_op", "estimated_bytes"]:
        values = frame[column].astype(float)
        if (~values.map(math.isfinite)).any() or (values <= 0).any():
            raise ValueError(f"column {column} contains non-positive or non-finite values")

    vector_heights = frame.loc[frame["implementation"] == "sorted_vector", "height"]
    tree_heights = frame.loc[frame["implementation"] != "sorted_vector", "height"]
    if vector_heights.notna().any() or tree_heights.isna().any() or (tree_heights <= 0).any():
        raise ValueError("height must be blank for vectors and positive for non-empty trees")

    trial_key = ["scenario", "distribution", "n", "trial", "seed"]
    group_sizes = frame.groupby(trial_key, dropna=False)["implementation"].nunique()
    if (group_sizes != len(IMPLEMENTATIONS)).any():
        raise ValueError("at least one trial is missing an implementation")
    checksum_counts = frame.groupby(trial_key, dropna=False)["checksum"].nunique()
    if (checksum_counts != 1).any():
        raise ValueError("implementations disagree on at least one trial checksum")

    duplicate_key = trial_key + ["implementation"]
    if frame.duplicated(duplicate_key).any():
        raise ValueError("duplicate benchmark trial rows found")

    expected_distributions = {
        "build": {"sorted", "random", "nearly_sorted"},
        "lookup": {"sorted", "random", "nearly_sorted"},
        "read_heavy": {"random"},
        "update_heavy": {"random"},
    }
    if set(frame["scenario"]) != set(expected_distributions):
        raise ValueError("benchmark CSV has missing or unknown scenarios")
    for scenario, distributions in expected_distributions.items():
        actual = set(frame.loc[frame["scenario"] == scenario, "distribution"])
        if actual != distributions:
            raise ValueError(f"scenario {scenario} has incorrect distributions: {sorted(actual)}")

    counts = frame.groupby(["scenario", "distribution", "implementation", "n"])["trial"].nunique()
    if counts.nunique() != 1:
        raise ValueError("benchmark points have inconsistent repetition counts")
    repetitions = int(counts.iloc[0])
    if expected_repetitions is not None and repetitions != expected_repetitions:
        raise ValueError(f"expected {expected_repetitions} repetitions, found {repetitions}")
    actual_sizes = set(int(value) for value in frame["n"].unique())
    if expected_sizes is not None and actual_sizes != expected_sizes:
        raise ValueError(f"expected sizes {sorted(expected_sizes)}, found {sorted(actual_sizes)}")


def summarize(frame: pd.DataFrame) -> pd.DataFrame:
    group_columns = ["scenario", "distribution", "implementation", "n"]
    grouped = frame.groupby(group_columns, as_index=False)
    summary = grouped.agg(
        repetitions=("trial", "nunique"),
        operations=("operations", "median"),
        elapsed_ns_median=("elapsed_ns", "median"),
        elapsed_ns_q1=("elapsed_ns", lambda values: values.quantile(0.25)),
        elapsed_ns_q3=("elapsed_ns", lambda values: values.quantile(0.75)),
        ns_per_op_median=("ns_per_op", "median"),
        ns_per_op_q1=("ns_per_op", lambda values: values.quantile(0.25)),
        ns_per_op_q3=("ns_per_op", lambda values: values.quantile(0.75)),
        height_median=("height", "median"),
        height_q1=("height", lambda values: values.quantile(0.25)),
        height_q3=("height", lambda values: values.quantile(0.75)),
        estimated_bytes_median=("estimated_bytes", "median"),
    )

    rbt = summary.loc[summary["implementation"] == "red_black_tree", group_columns[:-2] + ["n", "ns_per_op_median"]]
    rbt = rbt.rename(columns={"ns_per_op_median": "rbt_ns_per_op"})
    summary = summary.merge(rbt, on=["scenario", "distribution", "n"], how="left", validate="many_to_one")
    summary["speedup_vs_rbt"] = summary["rbt_ns_per_op"] / summary["ns_per_op_median"]
    summary = summary.drop(columns=["rbt_ns_per_op"])
    return summary.sort_values(group_columns).reset_index(drop=True)


def style_axis(axis: plt.Axes, title: str, ylabel: str) -> None:
    axis.set_title(title)
    axis.set_xlabel("Number of stored events (n)")
    axis.set_ylabel(ylabel)
    axis.grid(True, which="both", alpha=0.25)
    axis.set_xscale("log", base=2)


def plot_metric(
    axis: plt.Axes,
    data: pd.DataFrame,
    median_column: str,
    q1_column: str | None,
    q3_column: str | None,
    scale: float = 1.0,
) -> None:
    for implementation in IMPLEMENTATION_ORDER:
        subset = data.loc[data["implementation"] == implementation].sort_values("n")
        if subset.empty:
            continue
        x = subset["n"].to_numpy(dtype=float)
        y = subset[median_column].to_numpy(dtype=float) / scale
        axis.plot(
            x,
            y,
            color=COLORS[implementation],
            marker=MARKERS[implementation],
            linewidth=1.8,
            label=LABELS[implementation],
        )
        if q1_column and q3_column:
            lower = subset[q1_column].to_numpy(dtype=float) / scale
            upper = subset[q3_column].to_numpy(dtype=float) / scale
            axis.fill_between(x, lower, upper, color=COLORS[implementation], alpha=0.15)


def save_figure(figure: plt.Figure, path: Path) -> None:
    figure.savefig(path, dpi=180, bbox_inches="tight", facecolor="white")
    plt.close(figure)


def make_plots(summary: pd.DataFrame, figures_dir: Path) -> None:
    figures_dir.mkdir(parents=True, exist_ok=True)
    distributions = ["sorted", "random", "nearly_sorted"]
    repetitions = int(summary["repetitions"].iloc[0])

    figure, axes = plt.subplots(1, 3, figsize=(13.5, 4.2), sharey=False)
    for axis, distribution in zip(axes, distributions, strict=True):
        data = summary[(summary["scenario"] == "build") & (summary["distribution"] == distribution)]
        plot_metric(axis, data, "elapsed_ns_median", "elapsed_ns_q1", "elapsed_ns_q3", 1_000_000.0)
        style_axis(axis, distribution.replace("_", " ").title(), "Build time (ms)")
        axis.set_yscale("log")
    axes[0].legend(fontsize=8)
    figure.suptitle(f"Build performance by insertion order (median and IQR, {repetitions} trials)")
    figure.tight_layout()
    save_figure(figure, figures_dir / "build_time.png")

    figure, axes = plt.subplots(1, 3, figsize=(13.5, 4.2), sharey=False)
    for axis, distribution in zip(axes, distributions, strict=True):
        data = summary[(summary["scenario"] == "lookup") & (summary["distribution"] == distribution)]
        plot_metric(axis, data, "ns_per_op_median", "ns_per_op_q1", "ns_per_op_q3")
        style_axis(axis, distribution.replace("_", " ").title(), "Lookup time (ns/op)")
        axis.set_yscale("log")
    axes[0].legend(fontsize=8)
    figure.suptitle(f"Exact lookup performance (50% hits, median and IQR, {repetitions} trials)")
    figure.tight_layout()
    save_figure(figure, figures_dir / "lookup_time.png")

    figure, axes = plt.subplots(1, 2, figsize=(9.4, 4.2), sharey=False)
    for axis, scenario in zip(axes, ["read_heavy", "update_heavy"], strict=True):
        data = summary[summary["scenario"] == scenario]
        plot_metric(axis, data, "ns_per_op_median", "ns_per_op_q1", "ns_per_op_q3")
        title = "Read-heavy (80/10/10)" if scenario == "read_heavy" else "Update-heavy (20/40/40)"
        style_axis(axis, title, "Mixed workload time (ns/op)")
        axis.set_yscale("log")
    axes[0].legend(fontsize=8)
    figure.suptitle(f"Dynamic workloads: find/insert/erase (median and IQR, {repetitions} trials)")
    figure.tight_layout()
    save_figure(figure, figures_dir / "mixed_workloads.png")

    figure, axes = plt.subplots(1, 3, figsize=(13.5, 4.2), sharey=False)
    tree_summary = summary[summary["implementation"] != "sorted_vector"]
    for axis, distribution in zip(axes, distributions, strict=True):
        data = tree_summary[
            (tree_summary["scenario"] == "build") & (tree_summary["distribution"] == distribution)
        ]
        plot_metric(axis, data, "height_median", "height_q1", "height_q3")
        style_axis(axis, distribution.replace("_", " ").title(), "Tree height (nodes)")
        axis.set_yscale("log", base=2)
    axes[0].legend(fontsize=8)
    figure.suptitle(f"Tree height after construction (median and IQR, {repetitions} trials)")
    figure.tight_layout()
    save_figure(figure, figures_dir / "tree_height.png")

    figure, axis = plt.subplots(figsize=(6.8, 4.5))
    data = summary[(summary["scenario"] == "build") & (summary["distribution"] == "random")].copy()
    data["bytes_per_event"] = data["estimated_bytes_median"] / data["n"]
    plot_metric(axis, data, "bytes_per_event", None, None)
    style_axis(axis, "Random build", "Estimated structural bytes per event")
    axis.legend(fontsize=8)
    figure.suptitle("Estimated structural storage (excludes allocator metadata)")
    figure.tight_layout()
    save_figure(figure, figures_dir / "memory_per_event.png")


def write_metadata(path: Path, raw_path: Path, compiler: str, frame: pd.DataFrame) -> None:
    try:
        compiler_result = subprocess.run(
            [compiler, "--version"], check=True, capture_output=True, text=True, timeout=10
        )
        compiler_version = compiler_result.stdout.splitlines()[0]
    except (OSError, subprocess.SubprocessError, IndexError):
        compiler_version = "unavailable"
    metadata = {
        "benchmark_file": str(raw_path),
        "benchmark_sha256": hashlib.sha256(raw_path.read_bytes()).hexdigest(),
        "generated_utc": pd.Timestamp.now(tz="UTC").isoformat(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "processor": platform.processor() or "unavailable",
        "python": platform.python_version(),
        "compiler": compiler_version,
        "cxx_flags": "-std=c++23 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wshadow",
        "rows": int(len(frame)),
        "sizes": sorted(int(value) for value in frame["n"].unique()),
        "repetitions": int(frame.groupby(["scenario", "distribution", "implementation", "n"])["trial"].nunique().iloc[0]),
        "seed_min": int(frame["seed"].min()),
        "seed_max": int(frame["seed"].max()),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    frame = pd.read_csv(args.input)
    expected_sizes = None
    if args.expected_sizes:
        expected_sizes = {int(value) for value in args.expected_sizes.split(",")}
    validate_raw(frame, args.expected_repetitions, expected_sizes)
    summary = summarize(frame)
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary.to_csv(args.summary, index=False)
    make_plots(summary, args.figures_dir)
    write_metadata(args.metadata, args.input, args.compiler, frame)
    print(f"Validated {len(frame)} rows and wrote {len(summary)} summary rows.")
    print(f"Figures: {args.figures_dir}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, pd.errors.ParserError) as error:
        print(f"analyze_results: {error}", file=sys.stderr)
        raise SystemExit(1)
