#!/bin/sh

set -eu

HDUPES=${HDUPES:-./hdupes}
TMPDIR=${TMPDIR:-/tmp}
ROOT=$(mktemp -d "$TMPDIR/hdupes-test.XXXXXX")

cleanup() {
  chmod -R u+rwX "$ROOT" 2>/dev/null || true
  rm -rf "$ROOT"
}
trap cleanup EXIT HUP INT TERM

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

make_action_tree() {
  target=$1

  mkdir -p "$target/a" "$target/b"
  printf 'action duplicate one\n' > "$target/a/dup-one.txt"
  cp "$target/a/dup-one.txt" "$target/b/dup-two.txt"
  printf 'action duplicate three\n' > "$target/a/dup-three.txt"
  cp "$target/a/dup-three.txt" "$target/b/dup-four.txt"
  printf 'action unique\n' > "$target/a/unique.txt"
}

snapshot_tree() {
  target=$1
  (cd "$target" && find . -print | sort)
}

normalize_null_output() {
  tr '\0' '\n' < "$1" | sed '/^$/d' | sort
}

inode_of() {
  ls -id "$1" | awk '{ print $1 }'
}

mkdir -p "$ROOT/input/a" "$ROOT/input/b" "$ROOT/input/unique"
printf 'same duplicate payload\n' > "$ROOT/input/a/dup-one.txt"
cp "$ROOT/input/a/dup-one.txt" "$ROOT/input/b/dup-two.txt"
printf 'second duplicate payload\n' > "$ROOT/input/a/dup-three.txt"
cp "$ROOT/input/a/dup-three.txt" "$ROOT/input/b/dup-four.txt"
printf 'unique-by-size\n' > "$ROOT/input/unique/unreadable.txt"
chmod 000 "$ROOT/input/unique/unreadable.txt"
dd if=/dev/zero of="$ROOT/input/a/large-one.bin" bs=1024 count=256 2>/dev/null
cp "$ROOT/input/a/large-one.bin" "$ROOT/input/b/large-two.bin"
cp "$ROOT/input/a/large-one.bin" "$ROOT/input/unique/large-same-size-different-tail.bin"
printf 'tail' | dd of="$ROOT/input/unique/large-same-size-different-tail.bin" bs=1 seek=$((256 * 1024 - 4)) conv=notrunc 2>/dev/null

"$HDUPES" -q -0 -r "$ROOT/input" > "$ROOT/serial.out" 2> "$ROOT/serial.err"
"$HDUPES" -q -0 -r --threads=auto "$ROOT/input" > "$ROOT/auto.out" 2> "$ROOT/auto.err"
"$HDUPES" -q -0 -r --threads=1 --legacy-tree "$ROOT/input" > "$ROOT/single-legacy.out" 2> "$ROOT/single-legacy.err"
"$HDUPES" -q -0 -r --threads=4 "$ROOT/input" > "$ROOT/threaded.out" 2> "$ROOT/threaded.err"
"$HDUPES" -q -0 -r --threads=4 --legacy-tree "$ROOT/input" > "$ROOT/legacy-tree.out" 2> "$ROOT/legacy-tree.err"
"$HDUPES" -q -0 -r --threads=4 --experimental-pipeline "$ROOT/input" > "$ROOT/pipeline.out" 2> "$ROOT/pipeline.err"
"$HDUPES" -q -0 -r --threads=4 --hash-db="$ROOT/hashdb.txt" "$ROOT/input" > "$ROOT/hashdb-cold.out" 2> "$ROOT/hashdb-cold.err"
"$HDUPES" -q -0 -r --threads=4 --hash-db="$ROOT/hashdb.txt" "$ROOT/input" > "$ROOT/hashdb-warm.out" 2> "$ROOT/hashdb-warm.err"
"$HDUPES" -q -0 -r -u "$ROOT/input" > "$ROOT/unique-serial.out" 2> "$ROOT/unique-serial.err"
"$HDUPES" -q -0 -r -u --threads=4 "$ROOT/input" > "$ROOT/unique-threaded.out" 2> "$ROOT/unique-threaded.err"
"$HDUPES" -q -0 -r -u --threads=4 --experimental-pipeline "$ROOT/input" > "$ROOT/unique-pipeline.out" 2> "$ROOT/unique-pipeline.err"
"$HDUPES" -q -0 -r -m "$ROOT/input" > "$ROOT/summary-serial.out" 2> "$ROOT/summary-serial.err"
"$HDUPES" -q -0 -r -m --threads=4 "$ROOT/input" > "$ROOT/summary-threaded.out" 2> "$ROOT/summary-threaded.err"
"$HDUPES" -q -0 -r -m --threads=4 --experimental-pipeline "$ROOT/input" > "$ROOT/summary-pipeline.out" 2> "$ROOT/summary-pipeline.err"
"$HDUPES" -q -r -j "$ROOT/input" > "$ROOT/json-serial.out" 2> "$ROOT/json-serial.err"
"$HDUPES" -q -r -j --threads=4 "$ROOT/input" > "$ROOT/json-threaded.out" 2> "$ROOT/json-threaded.err"
"$HDUPES" -q -r -P fullhash --threads=4 "$ROOT/input" > "$ROOT/diagnostic-threaded.out" 2> "$ROOT/diagnostic-threaded.err"
"$HDUPES" -q -0 -r -X onlyext:txt --threads=4 --legacy-tree "$ROOT/input" > "$ROOT/filter-legacy.out" 2> "$ROOT/filter-legacy.err"
"$HDUPES" -q -0 -r -X onlyext:txt --threads=4 "$ROOT/input" > "$ROOT/filter-threaded.out" 2> "$ROOT/filter-threaded.err"

cmp "$ROOT/serial.out" "$ROOT/threaded.out" || fail "threaded output differs from serial output"
cmp "$ROOT/serial.out" "$ROOT/auto.out" || fail "default auto output differs from explicit --threads=auto output"
normalize_null_output "$ROOT/serial.out" > "$ROOT/serial.normalized"
normalize_null_output "$ROOT/single-legacy.out" > "$ROOT/single-legacy.normalized"
cmp "$ROOT/serial.normalized" "$ROOT/single-legacy.normalized" || fail "default auto output differs from single-thread legacy output set"
cmp "$ROOT/threaded.out" "$ROOT/legacy-tree.out" || fail "threaded pipeline output differs from legacy tree output"
cmp "$ROOT/threaded.out" "$ROOT/pipeline.out" || fail "experimental pipeline output differs from threaded output"
cmp "$ROOT/threaded.out" "$ROOT/hashdb-cold.out" || fail "hash DB cold output differs from threaded output"
cmp "$ROOT/threaded.out" "$ROOT/hashdb-warm.out" || fail "hash DB warm output differs from threaded output"
cmp "$ROOT/unique-serial.out" "$ROOT/unique-threaded.out" || fail "threaded unique output differs from serial output"
cmp "$ROOT/unique-threaded.out" "$ROOT/unique-pipeline.out" || fail "pipeline unique output differs from threaded output"
cmp "$ROOT/summary-serial.out" "$ROOT/summary-threaded.out" || fail "threaded summary output differs from serial output"
cmp "$ROOT/summary-threaded.out" "$ROOT/summary-pipeline.out" || fail "pipeline summary output differs from threaded output"
cmp "$ROOT/filter-legacy.out" "$ROOT/filter-threaded.out" || fail "threaded filter output differs from legacy tree output"

if ! grep -q '"matchSets"' "$ROOT/json-threaded.out"; then
  fail "threaded JSON output is missing matchSets"
fi

if ! grep -q 'large-one.bin' "$ROOT/json-threaded.out"; then
  fail "threaded JSON output is missing large duplicate"
fi

if ! grep -q 'Full hashes match' "$ROOT/diagnostic-threaded.out"; then
  fail "threaded diagnostic output did not use legacy matcher"
fi

if grep -q "unreadable.txt" "$ROOT/threaded.err"; then
  fail "threaded prehash read a unique-by-size file"
fi

if ! grep -q "dup-one.txt" "$ROOT/threaded.out"; then
  fail "expected duplicate group missing from threaded output"
fi

if ! grep -q "dup-three.txt" "$ROOT/threaded.out"; then
  fail "expected second duplicate group missing from threaded output"
fi

if ! grep -q "large-one.bin" "$ROOT/threaded.out"; then
  fail "expected large duplicate group missing from threaded output"
fi

if grep -q "large-same-size-different-tail.bin" "$ROOT/threaded.out"; then
  fail "same-size file with different tail was treated as duplicate"
fi

make_action_tree "$ROOT/perms"
chmod 600 "$ROOT/perms/a/dup-one.txt"
chmod 644 "$ROOT/perms/b/dup-two.txt"
"$HDUPES" -q -0 -r -p --threads=4 "$ROOT/perms" > "$ROOT/perms-threaded.out" 2> "$ROOT/perms-threaded.err"
if grep -q "dup-one.txt" "$ROOT/perms-threaded.out"; then
  fail "permission-sensitive threaded scan matched files with different modes"
fi

make_action_tree "$ROOT/delete-default"
make_action_tree "$ROOT/delete-pipeline"
"$HDUPES" -q -r -dN --threads=4 "$ROOT/delete-default" > "$ROOT/delete-default.out" 2> "$ROOT/delete-default.err"
"$HDUPES" -q -r -dN --threads=4 --experimental-pipeline "$ROOT/delete-pipeline" > "$ROOT/delete-pipeline.out" 2> "$ROOT/delete-pipeline.err"
snapshot_tree "$ROOT/delete-default" > "$ROOT/delete-default.tree"
snapshot_tree "$ROOT/delete-pipeline" > "$ROOT/delete-pipeline.tree"
cmp "$ROOT/delete-default.tree" "$ROOT/delete-pipeline.tree" || fail "pipeline delete result differs from threaded delete"
if [ "$(find "$ROOT/delete-pipeline" -type f | wc -l | tr -d ' ')" -ne 3 ]; then
  fail "pipeline delete did not preserve expected file count"
fi

make_action_tree "$ROOT/hardlink-default"
make_action_tree "$ROOT/hardlink-pipeline"
"$HDUPES" -q -r -L --threads=4 "$ROOT/hardlink-default" > "$ROOT/hardlink-default.out" 2> "$ROOT/hardlink-default.err"
"$HDUPES" -q -r -L --threads=4 --experimental-pipeline "$ROOT/hardlink-pipeline" > "$ROOT/hardlink-pipeline.out" 2> "$ROOT/hardlink-pipeline.err"
snapshot_tree "$ROOT/hardlink-default" > "$ROOT/hardlink-default.tree"
snapshot_tree "$ROOT/hardlink-pipeline" > "$ROOT/hardlink-pipeline.tree"
cmp "$ROOT/hardlink-default.tree" "$ROOT/hardlink-pipeline.tree" || fail "pipeline hardlink result differs from threaded hardlink"
if [ "$(inode_of "$ROOT/hardlink-pipeline/a/dup-one.txt")" != "$(inode_of "$ROOT/hardlink-pipeline/b/dup-two.txt")" ]; then
  fail "pipeline hardlink did not link first duplicate pair"
fi
if [ "$(inode_of "$ROOT/hardlink-pipeline/a/dup-three.txt")" != "$(inode_of "$ROOT/hardlink-pipeline/b/dup-four.txt")" ]; then
  fail "pipeline hardlink did not link second duplicate pair"
fi

make_action_tree "$ROOT/symlink-default"
make_action_tree "$ROOT/symlink-pipeline"
"$HDUPES" -q -r -l --threads=4 "$ROOT/symlink-default" > "$ROOT/symlink-default.out" 2> "$ROOT/symlink-default.err"
"$HDUPES" -q -r -l --threads=4 --experimental-pipeline "$ROOT/symlink-pipeline" > "$ROOT/symlink-pipeline.out" 2> "$ROOT/symlink-pipeline.err"
snapshot_tree "$ROOT/symlink-default" > "$ROOT/symlink-default.tree"
snapshot_tree "$ROOT/symlink-pipeline" > "$ROOT/symlink-pipeline.tree"
cmp "$ROOT/symlink-default.tree" "$ROOT/symlink-pipeline.tree" || fail "pipeline symlink result differs from threaded symlink"
if [ ! -L "$ROOT/symlink-pipeline/b/dup-two.txt" ]; then
  fail "pipeline symlink did not link first duplicate pair"
fi
if [ ! -L "$ROOT/symlink-pipeline/b/dup-four.txt" ]; then
  fail "pipeline symlink did not link second duplicate pair"
fi

make_action_tree "$ROOT/dedupe-default"
make_action_tree "$ROOT/dedupe-pipeline"
dedupe_supported=1
"$HDUPES" -q -r -B --threads=4 "$ROOT/dedupe-default" > "$ROOT/dedupe-default.out" 2> "$ROOT/dedupe-default.err" || {
  if grep -q "Operation not supported" "$ROOT/dedupe-default.err"; then
    dedupe_supported=0
    echo "SKIP: filesystem does not support block-level dedupe"
  else
    fail "dedupe run failed: $(cat "$ROOT/dedupe-default.err")"
  fi
}
if [ "$dedupe_supported" -eq 1 ]; then
  "$HDUPES" -q -r -B --threads=4 --experimental-pipeline "$ROOT/dedupe-pipeline" > "$ROOT/dedupe-pipeline.out" 2> "$ROOT/dedupe-pipeline.err"
  snapshot_tree "$ROOT/dedupe-default" > "$ROOT/dedupe-default.tree"
  snapshot_tree "$ROOT/dedupe-pipeline" > "$ROOT/dedupe-pipeline.tree"
  cmp "$ROOT/dedupe-default.tree" "$ROOT/dedupe-pipeline.tree" || fail "pipeline dedupe result differs from threaded dedupe"
  cmp "$ROOT/dedupe-pipeline/a/dup-one.txt" "$ROOT/dedupe-pipeline/b/dup-two.txt" || fail "pipeline dedupe changed first duplicate contents"
  cmp "$ROOT/dedupe-pipeline/a/dup-three.txt" "$ROOT/dedupe-pipeline/b/dup-four.txt" || fail "pipeline dedupe changed second duplicate contents"
fi

echo "OK"
