# Regression tests

There are three suites.

**Samples.** `baseline.sha256` records the SHA-256 of the SMF that each
`sample/*.mml` compiles to. `ctest` (and `make -C src check`) recompiles every
sample and compares against it, so any change that alters generated MIDI shows
up immediately. The samples are not bundled — see below.

**Bug fixes and new syntax.** `regress/*.mml` pins down the fixes for the
defects listed in `org-doc/todo.txt`, plus the MML this fork added, with
`regress.sha256` as the baseline. It runs through the same
`cmake/RunSampleTest.cmake` as the sample suite. `mod-chord.mml` covers the
`M` command combined with a zero-length chord, `psw-tempo.mml` a tempo set
across a `=1`/`=0` skip, and `psw-ichi.mml` an in-line `=1` leaking into the
next track. `tempo-order.mml` is the control: it exercises the neighbouring
cases those fixes must *not* change (multi-track tempo without any `=`,
relative/push/pop tempo, and `FT` displacements). `macro-long.mml`,
`macro-args.mml`, `bend-ij.mml` and `bend-range.mml` cover the mmlpp features
folded into the compiler — long macro names, macros with arguments, the `i`/`j`
bend-value functions, and where the bend range comes from. These MMLs were
written for this fork and carry no third-party content, so this suite always
runs.

The first three of those four are also checked against `mmlpp/mmlpp.pl`
directly, since mmlpp is the reference implementation of what they express:

```sh
perl mmlpp/mmlpp.pl test/regress/macro-args.mml > /tmp/pp.mml
src/mml2mid /tmp/pp.mml /tmp/a.mid
src/mml2mid test/regress/macro-args.mml /tmp/b.mid
cmp /tmp/a.mid /tmp/b.mid          # must be identical
```

The same holds for the sample MML embedded in `mmlpp/mmlppbnd.txt` (the lines
below its `cut here` marker). `bend-range.mml` is excluded because it uses
`#bendrange`, which mmlpp knows nothing about — that is the point of the file.

**Track names.** `trackname/*.mml` pins down the track-name grammar: the
parallel notation and wildcards inherited from the original, and the optional
second character this fork adds (`doc/CHANGES.md`). Each MML is paired with a
`.trk` file listing the tracks it must produce, in output order; the runner
also checks the track count in the SMF header. These MMLs were written for this
fork and carry no third-party content, so this suite always runs.

`trackname/parallel.mml` and `trackname/wildcompat.mml` use nothing but
original syntax and pass against the original 5.30b as well —
they are there to catch any drift in what the original could already express.

## The sample songs are not bundled

**`sample/` is not part of this fork.** The sample songs are third-party works:
their copyright belongs to each individual composer, and only the pieces by
A.Monden carry a free-use grant. This fork therefore does not redistribute
them. See the copyright section of the top-level `README.md`.

Without `sample/`, the build works normally and the test suite is simply
disabled — CMake prints a status line saying so.

To enable the suite, fetch the original mml2mid 5.30b archive and copy its
`sample/` directory into the root of this tree:

> <https://www.vector.co.jp/soft/dl/unix/art/se102432.html>

`baseline.sha256` is kept in the repository so the comparison still works when
you do. It expects the samples exactly as distributed — EUC-JP encoded and
byte-for-byte unmodified.

## Regenerating the bug-fix baseline

Only after deliberately changing generated output:

```sh
cd test/regress
for m in *.mml; do
    b=${m%.mml}
    ../../src/mml2mid "$m" /tmp/o.mid >/dev/null 2>&1 || continue
    printf '%s  %s\n' "$(sha256sum /tmp/o.mid | cut -d' ' -f1)" "$b"
done > ../regress.sha256
```

A change to `tempo-order.mml`'s hash is a red flag: that file exists precisely
to stay constant.

## How the tests run

Each sample is compiled with `sample/` as the working directory, because
`#include` inside an MML source is resolved relative to the process's current
directory, not relative to the including file. `08itsuka.mml` includes
`adc0804.mml` and fails from anywhere else.

## How the baseline was validated

The baseline is not self-referential. It was checked against the `.mid` files
that ship next to the samples, which were produced by the original pre-64-bit
program. At the modernisation commit, where behaviour was still identical to
the original:

**28 of the 32 samples were byte-for-byte identical to the shipped `.mid`.**

Three of this fork's later changes deliberately alter what four of the samples
compile to, so **25 match today** — see "Samples this fork changed on purpose"
below. That section is the audit trail for the remaining difference; the
statement above is what the baseline was originally checked against.

Reproducing that comparison needs two adjustments, because of how the
distribution was assembled — neither is a property of the code:

1. The shipped `*.mml` sources are **EUC-JP**, but the shipped `*.mid` files
   contain **Shift-JIS** text. The compiler copies string bytes through
   verbatim, so the samples have to be transcoded to Shift-JIS and compiled
   with `-m` to reproduce the shipped files.
2. `08itsuka.mml` must be compiled from within the sample directory so its
   `#include` resolves.

The four samples that still differ are explained, and none is a defect in the
build:

| Sample | Difference | Cause |
| --- | --- | --- |
| `00master` | many bytes | Uses `kr` (random velocity), seeded from `time()`. Output is nondeterministic by design and can never match a stored file. It is excluded from the baseline for the same reason — `rand()` also differs between C libraries. |
| `01hop2gs` | 1 byte | Tempo meta-event for `t166` |
| `01mkr39` | 2 bytes | Tempo meta-event for `t157` |
| `10kazeir` | 1 byte | Tempo meta-event for `t133` |

The three tempo cases are all the same thing. `tempo_conv()` in the current
source rounds to nearest, whereas the version that generated the shipped `.mid`
files truncated:

| MML | exact µs/quarter-note | shipped `.mid` (truncated) | this build (rounded) |
| --- | --- | --- | --- |
| `t166` | 361445.783 | 361445 | 361446 |
| `t157` | 382165.605 | 382165 | 382166 |
| `t133` | 451127.820 | 451127 | 451128 |

Only tempos that do not divide exactly are affected; `t120`, `t300` and the
like match. This is pre-existing skew between the shipped samples and the
shipped source, not a regression — `tempo_conv()` was additionally
differential-tested against the original implementation over 8,000,000
`(tempo_master, tempo)` combinations with zero mismatches.

## Samples this fork changed on purpose

Four samples no longer compile to what they did in the original, because they
exercise behaviour this fork deliberately changed. `baseline.sha256` records
the new output for these; the entries were regenerated when the discrepancy was
traced (2026-08-09). Nothing here is an encoding or line-ending artefact — the
samples are uniformly EUC-JP with LF line endings, and converting them to CRLF
changes no output byte.

| Sample | Change | Effect |
| --- | --- | --- |
| `02nm63` | Sub-tracks written *after* the main track on the same line are no longer dropped (a side effect of the two-character track-name work; called out in `doc/CHANGES.md` as the one change that can alter generated MIDI) | Line 46 reads `ZA1ACJBDMN…`, so sub-track `1A` now gets emitted. Track count 28 → 29 |
| `01mkr29` | `M` combined with a zero-length chord — one of the `org-doc/todo.txt` defects fixed | Delayed modulation is now emitted. +159 bytes |
| `01mkr39` | same | +2094 bytes |
| `01mkr40` | same | +525 bytes |

How this was established, in case it needs re-checking:

- Building the parent of the todo-fix commit (`30932dd`) reproduces the
  *previous* `baseline.sha256` for 31 of 32 samples, including all three
  `01mkr*`. Only `02nm63` differs there, because its change landed earlier with
  the track-name work.
- That build's output for the three `01mkr*` samples is exactly the size of the
  shipped `.mid` (31217 / 88980 / 8314 bytes).
- All four samples use `M`; none uses `=0` / `=1`, which rules out the other two
  todo-list fixes.

## Cross-platform agreement

The same baseline passes on both toolchains tested, so the two builds agree
byte-for-byte on all 32 samples:

- MSVC 19.51 / Windows x64 (Visual Studio 18)
- GCC 10 / Debian x86-64

## Regenerating the baseline

Only after deliberately changing generated output, and only from a build you
have verified:

```sh
cd sample
for m in *.mml; do
    b=${m%.mml}
    [ "$b" = 00master ] && continue     # nondeterministic
    ../src/mml2mid "$m" /tmp/o.mid >/dev/null 2>&1 || continue
    printf '%s  %s\n' "$(sha256sum /tmp/o.mid | cut -d' ' -f1)" "$b"
done > ../test/baseline.sha256
```
