#!/bin/sh
# Compile every test/regress/*.mml and compare against test/regress.sha256.
#
#   usage: test/run-regress.sh [path-to-mml2mid]
#
# These MMLs pin down the fixes for the defects listed in org-doc/todo.txt,
# plus the neighbouring cases those fixes must not change.  They are written
# for this fork and carry no third-party content, so unlike the sample suite
# this one always runs.
#
# The CMake build runs the same checks through cmake/RunSampleTest.cmake.

set -u

root=$(cd "$(dirname "$0")/.." && pwd)
exe=${1:-$root/src/mml2mid}
dir=$root/test/regress
tmp=${TMPDIR:-/tmp}/mml2mid-regress.$$.mid

if [ ! -x "$exe" ]; then
	echo "no such executable: $exe" >&2
	exit 2
fi
# Absolute, because each MML is compiled with test/regress/ as the working dir.
exe=$(cd "$(dirname "$exe")" && pwd)/$(basename "$exe")

ok=0
bad=0
badlist=

while read -r hash stem; do
	[ -n "${stem:-}" ] || continue
	rm -f "$tmp"
	if ! (cd "$dir" && "$exe" "$stem.mml" "$tmp") >/dev/null 2>&1; then
		bad=$((bad + 1)); badlist="$badlist $stem(exit)"; continue
	fi
	got=$(sha256sum "$tmp" | cut -d' ' -f1)
	if [ "$got" = "$hash" ]; then
		ok=$((ok + 1))
	else
		bad=$((bad + 1)); badlist="$badlist $stem"
	fi
done < "$root/test/regress.sha256"

rm -f "$tmp"
echo "$ok matched, $bad differed"
if [ $bad -ne 0 ]; then
	echo "differing:$badlist"
	exit 1
fi
