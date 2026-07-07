# Parallel Performance Roadmap

## Status

- Phase 1: benchmark harness - implemented
- Phase 2: regression fixtures for serial/threaded parity - implemented
- Phase 3: candidate-aware threaded hashing - implemented
- Phase 4: CLI wiring for `--threads=auto`, threaded pipeline, and
  `--legacy-tree` fallback - implemented
- Phase 5: bounded parallel byte confirmation - implemented
- Phase 6: hash DB warm-run benchmark support - implemented
- Phase 7: full array/group matching pipeline - default for threaded scans,
  with `--legacy-tree` fallback
- Phase 8: Rust rewrite gate/prototype decision - implemented; continue in C
  for now

## Current Engine

Threaded scans now use an adaptive worker count by default, capped at 8. They
walk directories with a bounded worker pool, sort collected files
deterministically by command-line root order and path, then avoid hashing
unique-size files. The threaded hash stage does this work:

1. collect files into an array
2. sort by file size
3. partial-hash only size groups with more than one file
4. full-hash only size+partial-hash groups with more than one file
5. keep byte-for-byte confirmation as the final safety check

On macOS, the parallel walker first tries `getattrlistbulk()` for batched
directory entry enumeration and falls back to `readdir()` if that path is not
available or returns an unexpected layout. Large matched pairs are confirmed
with a bounded `pread()` worker pool when `--threads` is active. Small pairs
and non-threaded scans use the serial confirmation fallback. `--recurse:`
continues to use the legacy traversal path so its cross-range traversal-check
semantics remain unchanged.

## Current Pipeline

Non-threaded scans still use the matching tree in `hdupes.c` and `match.c`.
Threaded scans now use an array/group pipeline by default, unless
`--legacy-tree` is specified:

1. preserve original file-list order for output compatibility
2. group candidates by size
3. group survivors by partial hash
4. group survivors by full hash
5. apply pair conditions such as `--isolate`, `--one-file-system`,
   `--permissions`, and hardlink handling
6. confirm bytes with the parallel confirmer
7. register duplicate chains in the same shape consumed by existing actions

## Remaining Hardening

The implemented phases are covered by local regression and benchmark smoke
tests. Additional hardening should expand golden output tests around
diagnostics, hardlink triangles, symlink loops, larger filter matrices, and
platform-specific filesystems.
