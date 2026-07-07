#!/bin/sh
set -eu

root_dir="$(cd "$(dirname "$0")/.." && pwd)"
formula="$root_dir/Formula/hdupes.rb"
version_file="$root_dir/version.h"

ver="$(rg -n '#define VER ' "$version_file" | sed -E 's/.*"([^"]+)".*/\1/')"
if [ -z "$ver" ]; then
  echo "Failed to read version from $version_file" >&2
  exit 1
fi

tag="v$ver"
url="https://github.com/dj-skn/hdupes/archive/refs/tags/$tag.tar.gz"

sha="$(curl -L "$url" | shasum -a 256 | awk '{print $1}')"
if [ -z "$sha" ]; then
  echo "Failed to compute SHA for $url" >&2
  exit 1
fi

sed -E -i '' \
  -e "s|^  url \".*\"|  url \"$url\"|" \
  -e "s|^  sha256 \".*\"|  sha256 \"$sha\"|" \
  "$formula"

echo "Updated formula:"
echo "  url: $url"
echo "  sha256: $sha"
