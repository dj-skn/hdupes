#!/bin/bash

# Runs the installed *dupes* binary and the built binary and compares
# the output for sameness. Also displays timing statistics.

ERR=0

# Detect installed hdupes
if [ -z "$ORIG_HDUPES" ]
	then
	hdupes -v 2>/dev/null >/dev/null && ORIG_HDUPES=hdupes
	test ! -z "$WINDIR" && "$WINDIR/hdupes.exe" -v 2>/dev/null >/dev/null && ORIG_HDUPES="$WINDIR/hdupes.exe"
	[ -z "$ORIG_HDUPES" ] && echo "error: can't find old hdupes; set ORIG_HDUPES manually" >&2 && exit 1
fi

if [ ! $ORIG_HDUPES -v 2>/dev/null >/dev/null ]
	then echo "Can't run installed hdupes"
	echo "To manually specify an original hdupes, use: ORIG_HDUPES=path/to/hdupes $0"
	exit 1
fi

test ! -e ./hdupes && echo "Build hdupes first, silly" && exit 1

echo -n "Installed $ORIG_HDUPES:"
sync
time $ORIG_HDUPES -q "$@" > installed_output.txt || ERR=1
echo -en "\nBuilt hdupes:"
sync
time ./hdupes -q "$@" > built_output.txt || ERR=1
diff -Nau installed_output.txt built_output.txt

rm -f installed_output.txt built_output.txt
test "$ERR" != "0" && echo "Errors were returned during execution"
