#!/bin/sh
set -eu

prefix="${1:-$HOME/.local}"
brew_prefix="$(brew --prefix libjodycode)"
script_dir="$(cd "$(dirname "$0")" && pwd)"

echo "Building hdupes with libjodycode from: $brew_prefix"
make -C "$script_dir" clean
make -C "$script_dir" CFLAGS_EXTRA="-I$brew_prefix/include" LDFLAGS_EXTRA="-L$brew_prefix/lib"

echo "Build complete. Version:"
"$script_dir/hdupes" -v | head -n 4

echo "Installing to: $prefix"
make -C "$script_dir" install PREFIX="$prefix"
