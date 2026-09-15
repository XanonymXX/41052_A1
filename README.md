# 41052 Assignment 1: Ordered Event Sets

This C++23 project implements and empirically compares three event-scheduler ordered sets:

- a hand-written red-black tree;
- an ordinary, unbalanced binary search tree; and
- a sorted vector.

Events are ordered by `(timestamp, id)`, so events may share a timestamp while remaining unique.
The repository includes deterministic correctness tests, a reproducible Track A benchmark, raw and
summarised results, five plots, an English report, and a video walkthrough outline.

## Repository map

- `include/` and `src/`: public interface, three implementations, and demonstration program
- `tests/`: behavioural, invariant, structural, and analysis tests
- `benchmark/`: C++ benchmark harness and deterministic workload generation
- `scripts/`: CSV validation, statistics, and plotting
- `results/`: raw trials, statistical summary, environment metadata, and figures
- `report/`: editable English report and video outline
- `output/pdf/report.pdf` and `output/word/report.docx`: report documents

## Requirements

- A C++23 compiler (`clang++` or `g++`)
- GNU Make
- Python 3.10 or newer

Create the Python environment once:

```sh
make setup
```

The compatible dependency ranges are listed in `requirements.txt`.

## Build and run the demonstration

```sh
make demo
./build/scheduler_demo
```

The demonstration inserts simultaneous and non-simultaneous events, prints them in chronological
order, and runs a next-event query with each implementation.

## Correctness tests

```sh
make test
make sanitize
```

The C++ suite checks all implementations against `std::map` for 10,000 deterministic random
operations. It tests duplicates, missing keys, bounds, extreme timestamps, deletion shapes, clear,
ordered output, and sorted adversarial input. Red-black invariants are checked after every mutation.
The sanitizer target uses UndefinedBehaviorSanitizer. AddressSanitizer leak detection was not
supported on the recorded macOS environment, so this repository does not claim an ASan result.

## Benchmark

Run the fast end-to-end smoke test first:

```sh
make test-analysis
```

Run the preregistered full experiment and regenerate all results:

```sh
make benchmark
make plots
```

The full experiment uses sizes 250 through 8,000, one warm-up, seven measured trials, and seed
41052. `results/raw.csv` is the source of truth. `scripts/analyze_results.py` validates its schema,
checks cross-implementation checksums, computes median/IQR summaries, records environment metadata,
and creates five figures. Input generation, validation, and file output are outside timed regions.

The benchmark executable can also be run directly:

```sh
./build/benchmark --help
./build/benchmark --quick --repetitions 3 --seed 41052 --output build/custom.csv
```
