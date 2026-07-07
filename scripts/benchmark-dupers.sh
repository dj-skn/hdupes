#!/bin/sh

set -eu

TARGET=${1:-testdir}
HDUPES=${HDUPES:-./hdupes}
THREADS=${THREADS:-auto}
RUNS=${RUNS:-3}
HASHDB=${HASHDB:-}
PIPELINE=${PIPELINE:-0}

if [ ! -x "$HDUPES" ]; then
  echo "error: HDUPES is not executable: $HDUPES" >&2
  exit 1
fi

if [ ! -e "$TARGET" ]; then
  echo "error: target does not exist: $TARGET" >&2
  exit 1
fi

bench() {
  name=$1
  shift

  if ! command -v "$1" >/dev/null 2>&1 && [ ! -x "$1" ]; then
    printf '%-18s skipped\n' "$name"
    return
  fi

  i=1
  while [ "$i" -le "$RUNS" ]; do
    printf '%-18s run %d/%d: ' "$name" "$i" "$RUNS"
    /usr/bin/time -p "$@" >/dev/null
    i=$((i + 1))
  done
}

echo "Target: $TARGET"
echo "Runs: $RUNS"
echo "Threads: $THREADS"
if [ "$PIPELINE" != "0" ]; then
  echo "Experimental pipeline: yes"
fi
if [ -n "$HASHDB" ]; then
  echo "Hash DB: $HASHDB"
fi
echo

bench "hdupes-serial" "$HDUPES" -q -r --threads=1 --legacy-tree "$TARGET"
bench "hdupes-threaded" "$HDUPES" -q -r --threads="$THREADS" "$TARGET"
if [ "$PIPELINE" != "0" ]; then
  bench "hdupes-pipeline" "$HDUPES" -q -r --threads="$THREADS" --experimental-pipeline "$TARGET"
fi
if [ -n "$HASHDB" ]; then
  rm -f "$HASHDB"
  "$HDUPES" -q -r --threads="$THREADS" --hash-db="$HASHDB" "$TARGET" >/dev/null
  bench "hdupes-hashdb" "$HDUPES" -q -r --threads="$THREADS" --hash-db="$HASHDB" "$TARGET"
fi
bench "jdupes" jdupes -q -r "$TARGET"
bench "fdupes" fdupes -q -r "$TARGET"
bench "ffdupes" ffdupes -q -r "$TARGET"
