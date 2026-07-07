#!/bin/sh

set -eu

OUT=${1:-/tmp/hdupes-bench-tree}
UNIQUE_COUNT=${UNIQUE_COUNT:-2000}
DUP_GROUPS=${DUP_GROUPS:-200}
DUP_COPIES=${DUP_COPIES:-3}
LARGE_GROUPS=${LARGE_GROUPS:-20}
LARGE_KIB=${LARGE_KIB:-1024}

rm -rf "$OUT"
mkdir -p "$OUT/unique" "$OUT/dupes" "$OUT/large"

i=1
while [ "$i" -le "$UNIQUE_COUNT" ]; do
  printf 'unique-%08d\n' "$i" > "$OUT/unique/file-$i.txt"
  i=$((i + 1))
done

g=1
while [ "$g" -le "$DUP_GROUPS" ]; do
  payload="$OUT/dupes/group-$g.payload"
  printf 'duplicate-group-%08d\n' "$g" > "$payload"

  c=1
  while [ "$c" -le "$DUP_COPIES" ]; do
    cp "$payload" "$OUT/dupes/group-$g-copy-$c.txt"
    c=$((c + 1))
  done
  rm "$payload"
  g=$((g + 1))
done

g=1
while [ "$g" -le "$LARGE_GROUPS" ]; do
  payload="$OUT/large/group-$g.payload"
  dd if=/dev/zero of="$payload" bs=1024 count="$LARGE_KIB" 2>/dev/null
  printf 'large-duplicate-group-%08d\n' "$g" >> "$payload"

  c=1
  while [ "$c" -le "$DUP_COPIES" ]; do
    cp "$payload" "$OUT/large/group-$g-copy-$c.bin"
    c=$((c + 1))
  done
  rm "$payload"
  g=$((g + 1))
done

echo "$OUT"
