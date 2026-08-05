#!/bin/sh
# Compile every sample and compare against the recorded SHA-256 baseline.
#
#   usage: test/run-baseline.sh [path-to-mml2mid]
#
# Must be able to find sample/ and test/baseline.sha256 relative to the repo
# root.  The compiler is run with the sample directory as its working
# directory, because `#include' inside an MML source is resolved relative to
# the process's current directory.

set -u

root=$(cd "$(dirname "$0")/.." && pwd)
exe=${1:-$root/src/mml2mid}
tmp=${TMPDIR:-/tmp}/mml2mid-baseline.$$.mid

if [ ! -x "$exe" ]; then
	echo "no such executable: $exe" >&2
	exit 2
fi
# Absolute, because each sample is compiled with sample/ as the working dir.
exe=$(cd "$(dirname "$exe")" && pwd)/$(basename "$exe")

ok=0
bad=0
badlist=

while read -r hash stem; do
	[ -n "${stem:-}" ] || continue
	rm -f "$tmp"
	if ! (cd "$root/sample" && "$exe" "$stem.mml" "$tmp") >/dev/null 2>&1; then
		bad=$((bad + 1)); badlist="$badlist $stem(exit)"; continue
	fi
	got=$(sha256sum "$tmp" | cut -d' ' -f1)
	if [ "$got" = "$hash" ]; then
		ok=$((ok + 1))
	else
		bad=$((bad + 1)); badlist="$badlist $stem"
	fi
done < "$root/test/baseline.sha256"

rm -f "$tmp"
echo "$ok matched, $bad differed"
if [ $bad -ne 0 ]; then
	echo "differing:$badlist"
	exit 1
fi
