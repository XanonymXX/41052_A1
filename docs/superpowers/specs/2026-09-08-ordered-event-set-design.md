# Ordered Event Set: Three-Implementation Design

## Goal

Implement the data-structure layer for an event scheduler using three interchangeable ordered-set backends. This provides the foundation for a later empirical comparison of time, space, tree height, comparisons, and red-black-tree balancing work.

## Data Model and Interface

- An `Event` contains an integer timestamp, an integer ID, and a title.
- Events are ordered by `(timestamp, id)`. The composite key makes each event unique while allowing multiple events at the same timestamp.
- Every implementation exposes the same operations: insert, erase by key, exact lookup, first event at or after a timestamp, ordered traversal, size, empty, clear, height, and estimated memory use.
- Inserting an existing `(timestamp, id)` returns `false` and leaves the stored event unchanged. Missing lookups and erasures return an empty result or `false`.

## Implementations

1. `RedBlackTreeEventSet`: a pointer-based red-black tree with a black sentinel leaf. Insert and delete fix-up preserve root-black, no-red-red, and equal-black-height invariants. A validation method is exposed for tests.
2. `BinarySearchTreeEventSet`: an unbalanced pointer-based BST with identical ordering and observable behavior. Sorted input is intentionally allowed to create a linear-height tree.
3. `SortedVectorEventSet`: a contiguous sorted vector using binary search. Lookup is logarithmic; insertion and deletion shift elements and are linear.

Each implementation owns its data, supports destruction and `clear`, and disables copying to avoid ambiguous deep-copy behavior in this assignment slice.

## Program Structure

- Public headers contain the event model, common interface, and three class declarations.
- Source files contain the tree algorithms and vector implementation.
- A small CLI demonstration inserts sample scheduler events and prints each implementation in order.
- A standalone test executable runs the same behavioral suite against all three versions, plus targeted red-black-tree invariant and adversarial-order tests.

## Correctness and Failure Handling

- Empty structures must safely handle lookup, lower-bound, erase, traversal, clear, and destruction.
- Deleting a leaf, a node with one child, a node with two children, and the root are covered.
- Duplicate insertion must not change size or overwrite data.
- Integer timestamp extremes are valid; no arithmetic on key fields is used during comparison.

## Build and Verification

- Use portable C++21 and a simple `Makefile`, since CMake is not installed in the current environment.
- Compile with strict warnings.
- Run deterministic unit tests and an AddressSanitizer/UndefinedBehaviorSanitizer build where supported.
- Benchmark generation and empirical reporting are intentionally deferred until the three implementations are correct.
