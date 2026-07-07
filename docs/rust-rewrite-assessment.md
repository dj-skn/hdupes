# Rust Rewrite Assessment

## Decision

Keep the performance engine in C for now. Do not start a Rust rewrite.

## Recommendation

Do not start with a full Rust rewrite. First prove the staged parallel scan
architecture in the existing C implementation, then decide whether Rust is
worth the migration cost.

## Why

The main speed gains come from avoiding unnecessary I/O:

- group files by size before hashing
- partial-hash only duplicate-size candidates
- full-hash only size+partial-hash candidate groups
- confirm bytes only inside full-hash groups

Rust can make those stages easier to express safely with worker pools and
channels, but it will not make a broad, unnecessary hashing pass fast enough
by itself.

## Rust Advantages

- safer parallelism for traversal, hashing, and confirmation workers
- easier data-oriented grouping with vectors and slices
- strong ownership boundaries for destructive actions
- straightforward benchmark and property-test harnesses
- mature crates for walking, hashing, and parallel iteration

Candidate crates:

- `rayon` for data-parallel grouping and hashing
- `ignore` or `walkdir` for traversal
- `xxhash-rust` for fast non-cryptographic hashing
- `serde_json` for JSON output
- `clap` for CLI compatibility

## Rust Risks

- destructive actions must be revalidated from scratch
- output ordering and compatibility need golden tests
- platform-specific dedupe and clone APIs still need unsafe or FFI code
- Windows, APFS, BTRFS, XFS, hardlink, symlink, and TOCTTOU behavior all need
  parity tests
- a rewrite can be slower if it preserves the wrong algorithm

## Decision Gate

Consider a Rust rewrite only after the C implementation has:

- a benchmark harness with repeatable datasets (implemented)
- serial/threaded correctness parity tests (implemented)
- candidate-aware parallel hashing (implemented)
- parallel byte confirmation or a measured reason not to use it (implemented
  in the C engine for threaded large-file matches)
- warm-run hash database benchmarks (implemented)
- compatibility tests for print, JSON, summarize, delete, hardlink, symlink,
  and dedupe (implemented for local fixtures)
- compatibility tests for broader filter and platform edge cases (pending)

The current implementation clears the main performance gate without a rewrite.
Rust should be reconsidered only if the remaining work becomes primarily about
maintaining complex concurrency or platform-specific safety boundaries.
