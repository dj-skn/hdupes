# hdupes

Fast, safe duplicate-file finder with optional actions.

Forked/inspired from `jdupes` and `ddupes`.

## Contents
- Quick start
- Features
- Usage
- Install (local build)
- Safety notes
- Flags

## Quick start
```
hdupes PATH...
hdupes -r PATH...
hdupes -r --threads=4 PATH...
```

## Features
- Accurate matching pipeline: size -> partial hash -> full hash -> byte compare
- Actions: print, summarize, delete, hardlink, symlink, dedupe
- JSON output and hash DB support
- Pre-hash threading with `--threads N` (hashing stage only)
- Flexible filters by size, extension, substring, and date
- Safe defaults and traversal safety checks

## Usage
```
hdupes [options] PATH...
```

Examples:
```
hdupes -r ~/Pictures
hdupes -r --threads=4 ~/Pictures
hdupes -m ~/Pictures
hdupes -d ~/Downloads
```

## Safety notes
- Default mode only prints matches.
- `-Q` and `-T` bypass full verification and can cause data loss.
- `-U` disables traversal safety checks; use only if you know why.

## Install (local build)
Requirements: `libjodycode`

Homebrew (tap):
```
brew tap dj-skn/hdupes
brew install hdupes
```

Homebrew:
```
brew install libjodycode
```

Build:
```
make CFLAGS_EXTRA="-I/opt/homebrew/include" LDFLAGS_EXTRA="-L/opt/homebrew/lib"
```

Install to `~/.local`:
```
make install PREFIX="$HOME/.local"
```

## Flags
- `-@ --loud` output low-level debug info while running
- `-0 --print-null` output nulls instead of CR/LF (like `find -print0`)
- `-1 --one-file-system` do not match files on different filesystems/devices
- `-A --no-hidden` exclude hidden files from consideration
- `-B --dedupe` copy-on-write deduplication (reflink/clone)
- `-C --chunk-size=#` override I/O chunk size (KiB)
- `-D --debug` output debug stats after completion
- `-d --delete` prompt to delete duplicates
- `-e --error-on-dupe` exit on any duplicate found with status code 255
- `-f --omit-first` omit the first file in each set of matches
- `-H --hard-links` treat hard-linked files as duplicates
- `-h --help` display help
- `-i --reverse` reverse sort order
- `-I --isolate` files in the same specified directory won't match
- `-j --json` produce JSON output
- `-J --threads=#` pre-hash files using N threads (hashing stage only)
- `-l --link-soft` make relative symlinks for duplicates without prompting
- `-L --link-hard` hard link duplicates without prompting
- `-m --summarize` summarize dupe information
- `-M --print-summarize` print matches and summarize at the end
- `-N --no-prompt` with `--delete`, keep first file and delete the rest
- `-O --param-order` parameter order is more important than selected sort
- `-o --order=BY` sort by `name` (default) or `time`
- `-p --permissions` owner/group/permission bits must match
- `-P --print=type` print extra info (`partial`, `early`, `fullhash`)
- `-q --quiet` hide progress indicator
- `-Q --quick` skip byte-for-byte verification (dangerous)
- `-r --recurse` process subdirectories
- `-R --recurse:` process subdirectories only after this option
- `-s --symlinks` follow symlinks
- `-S --size` show size of duplicate files
- `-t --no-change-check` disable TOCTTOU safety check
- `-T --partial-only` match on partial hashes only (dangerous; must be repeated)
- `-u --print-unique` print only unique files
- `-U --no-trav-check` disable double-traversal safety check
- `-v --version` display version and build info
- `-X --ext-filter=x:y` filter files based on criteria
- `-y --hash-db=file` use a hash database text file
- `-z --zero-match` consider zero-length files duplicates
- `-Z --soft-abort` on CTRL-C, act on matches so far

## License
MIT. See `LICENSE.txt`.
