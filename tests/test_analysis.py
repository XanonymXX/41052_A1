#!/usr/bin/env python3
"""Small deterministic tests for benchmark CSV validation and summary statistics."""

from __future__ import annotations

import importlib.util
from pathlib import Path

import pandas as pd


def load_analysis_module():
    path = Path(__file__).resolve().parents[1] / "scripts" / "analyze_results.py"
    specification = importlib.util.spec_from_file_location("analyze_results", path)
    if specification is None or specification.loader is None:
        raise RuntimeError("could not load analyze_results.py")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def make_fixture() -> pd.DataFrame:
    rows = []
    scenarios = {
        "build": ["sorted", "random", "nearly_sorted"],
        "lookup": ["sorted", "random", "nearly_sorted"],
        "read_heavy": ["random"],
        "update_heavy": ["random"],
    }
    implementations = ["red_black_tree", "ordinary_bst", "sorted_vector"]
    base_times = {"red_black_tree": 20.0, "ordinary_bst": 40.0, "sorted_vector": 10.0}
    for scenario, distributions in scenarios.items():
        for distribution in distributions:
            for trial, factor in enumerate([0.5, 1.0, 1.5]):
                for implementation in implementations:
                    ns_per_op = base_times[implementation] * factor
                    rows.append(
                        {
                            "scenario": scenario,
                            "distribution": distribution,
                            "implementation": implementation,
                            "n": 100,
                            "trial": trial,
                            "seed": 41052 + trial,
                            "operations": 100,
                            "elapsed_ns": int(ns_per_op * 100),
                            "ns_per_op": ns_per_op,
                            "height": None if implementation == "sorted_vector" else 10,
                            "estimated_bytes": 6400,
                            "checksum": 9000 + trial,
                        }
                    )
    return pd.DataFrame(rows)


def main() -> int:
    analysis = load_analysis_module()
    fixture = make_fixture()
    analysis.validate_raw(fixture, expected_repetitions=3)
    summary = analysis.summarize(fixture)

    row = summary[
        (summary["scenario"] == "build")
        & (summary["distribution"] == "random")
        & (summary["implementation"] == "ordinary_bst")
    ].iloc[0]
    assert row["ns_per_op_median"] == 40.0
    assert row["ns_per_op_q1"] == 30.0
    assert row["ns_per_op_q3"] == 50.0
    assert row["speedup_vs_rbt"] == 0.5

    broken_checksum = fixture.copy()
    broken_checksum.loc[0, "checksum"] = -1
    try:
        analysis.validate_raw(broken_checksum, expected_repetitions=3)
    except ValueError as error:
        assert "checksum" in str(error)
    else:
        raise AssertionError("checksum mismatch was not rejected")

    duplicated = pd.concat([fixture, fixture.iloc[[0]]], ignore_index=True)
    try:
        analysis.validate_raw(duplicated, expected_repetitions=3)
    except ValueError as error:
        assert "duplicate" in str(error)
    else:
        raise AssertionError("duplicate trial was not rejected")

    print("Analysis tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

