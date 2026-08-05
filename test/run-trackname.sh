#!/bin/sh
# Compile every test/trackname/*.mml and check which tracks it produced.
#
#   usage: test/run-trackname.sh [path-to-mml2mid]
#
# The expected list lives next to the MML as a .trk file: the track names in
# output order, separated by whitespace.  mml2mid reports them on stderr as
# "trk 0A0:   96 steps".  These MMLs are written for this fork and carry no
# third-party content, so unlike the sample suite this one always runs.
#
# The CMake build runs the same checks through cmake/RunTrackNameTest.cmake,
# which additionally verifies the track count in the SMF header.

set -u

root=$(cd "$(dirname "$0")/.." && pwd)
exe=${1:-$root/src/mml2mid}
dir=$root/test/trackname
tmp=${TMPDIR:-/tmp}/mml2mid-trackname.$$.mid

if [ ! -x "$exe" ]; then
	echo "no such executable: $exe" >&2
	exit 2
fi
exe=$(cd "$(dirname "$exe")" && pwd)/$(basename "$exe")

ok=0
bad=0
badlist=

for mml in "$dir"/*.mml; do
	stem=$(basename "$mml" .mml)
	rm -f "$tmp"

	got=$("$exe" "$mml" "$tmp" 2>&1 |
	      sed -n 's/^trk \([0-9A-Z]*\):.*/\1/p' | tr '\n' ' ')
	if [ ! -f "$tmp" ]; then
		bad=$((bad + 1)); badlist="$badlist $stem(no-output)"; continue
	fi
	want=$(tr -s ' \t\n' ' ' < "$dir/$stem.trk")

	# Compare with surrounding blanks collapsed, so trailing whitespace in
	# either list does not matter.
	if [ "$(echo $got)" = "$(echo $want)" ]; then
		ok=$((ok + 1))
	else
		bad=$((bad + 1)); badlist="$badlist $stem"
		echo "$stem.mml:" >&2
		echo "  produced: $(echo $got)" >&2
		echo "  expected: $(echo $want)" >&2
	fi
done

rm -f "$tmp"
echo "$ok matched, $bad differed"
if [ $bad -ne 0 ]; then
	echo "differing:$badlist"
	exit 1
fi
