#!/bin/sh
set -eu

prefix="${1:-$HOME/.local}"
update_formula=0
if [ "${2:-}" = "--update-formula" ]; then
  update_formula=1
fi

brew_prefix="$(brew --prefix libjodycode)"
script_dir="$(cd "$(dirname "$0")" && pwd)"

echo "Building hdupes with libjodycode from: $brew_prefix"
make -C "$script_dir" clean
make -C "$script_dir" CFLAGS_EXTRA="-I$brew_prefix/include" LDFLAGS_EXTRA="-L$brew_prefix/lib"

echo "Build complete. Version:"
"$script_dir/hdupes" -v | head -n 4

echo "Installing to: $prefix"
make -C "$script_dir" install PREFIX="$prefix"

if [ "$update_formula" -eq 1 ]; then
  echo "Updating Homebrew formula SHA for current tag..."
  "$script_dir/scripts/update-formula-sha.sh"
fi
