# Building mml2mid

This repository is a **fork of mml2mid Version 5.30b** by A.Monden, H.Fujii
(MKR), H.Kuroda and N.Nide. The original sources came from:

> <https://www.vector.co.jp/soft/dl/unix/art/se102432.html>

64-bit builds for Windows, Linux and macOS. No configuration switches are
needed — the platform is detected automatically.

## CMake (all platforms, recommended)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run the regression tests (see `test/README.md`):

```sh
cd build && ctest            # CMake < 3.20
ctest --test-dir build       # CMake >= 3.20
```

On Windows with Visual Studio, the generator is multi-config:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Install with `cmake --install build --prefix /usr/local`.

## Makefile (Linux, macOS, *BSD, MinGW)

```sh
cd src
make
make check          # compile every sample and compare hashes
sudo make install   # PREFIX=/usr/local by default
```

## Toolchains verified

| Toolchain | Platform | Result |
| --- | --- | --- |
| MSVC 19.51 (VS 18) | Windows x64 | builds clean at `/W3`, 33/33 tests |
| GCC 10 | Debian x86-64 | builds clean at `-O2 -Wall -Wextra`, 33/33 tests |
| clang 21 (clang-cl) | Windows x64 | no errors or substantive warnings |

Both full builds produce byte-identical MIDI on all 32 deterministic samples.

## Requirements

- A C11 compiler. `gnu11` is selected by default because the program uses
  POSIX (`open`, `read`, `isatty`, `strdup`) and glibc hides those under a
  strict `-std=c11`. `src/compat.h` also sets the feature-test macros itself,
  so a strict build works too.
- Nothing else. There are no external dependencies.

## Source encoding

The original distribution was EUC-JP throughout. Sources and documentation are
now **UTF-8**; the compiler's input data is left alone.

| What | Encoding | Note |
| --- | --- | --- |
| `src/*.c`, `src/*.h` | UTF-8 | all non-ASCII was in Japanese comments, so no program text changed |
| documentation (`*.txt`, `src/mml2mid.1`) | UTF-8 | converted with the character sequence verified identical |
| `sample/*.mml`, `doc/tr-rack.mml` | **still EUC-JP** | see below |
| `doc/mml2mid.def` | untouched | VZ editor definition file, contains binary |
| `mmlpp/mmlpp.pl`, `tk/tkmml2mid.tcl` | still EUC-JP | programs, not documentation |

`.mml` files are compiler *input*, not documentation. mml2mid copies string
bytes straight into SMF meta events (song title, track names), so re-encoding
a `.mml` changes the bytes of the MIDI it produces — which would break both the
match against the shipped `.mid` files and the regression baseline. Use the
`-m` switch to select how strings are interpreted instead.

The build files pass `/utf-8` to MSVC and `-finput-charset=UTF-8` to
GCC/Clang. If you compile by hand with `cl`, pass `/utf-8` or you will get
C4819 warnings.

## What changed in the modernisation

Behaviour is unchanged — the compiler produces the same MIDI as before (see
`test/README.md` for the evidence).

### Build system

- Added `CMakeLists.txt` (MSVC / GCC / Clang) and rewrote `src/makefile` as a
  portable POSIX makefile with `install` and `check` targets.
- Dropped `-DUNIX`; the platform is detected from `_WIN32`.
- The legacy `makefile.bcc`, `makefile.egc`, `makefile.lcc` and `file.asm`
  (16-bit DOS assembler) are no longer referenced by any build. They are left
  in place as historical artifacts and can be deleted.

### 64-bit correctness

- `Fpos_t` was `long`. File positions are computed as pointer differences, so
  on Win64 (where `long` is 32 bits) every position was being truncated. It is
  now `ptrdiff_t`, and `fseek2`/`ftell2`/`smftrkend` follow suit.
- `qsort()` was called with a comparator cast to an unprototyped function
  pointer type — undefined behaviour, and rejected outright by newer standards.
  The comparator now has the correct `(const void *, const void *)` signature.

### Portability layer

- `win.h` became `compat.h`: a real Win32/POSIX split instead of the old
  hand-set `UNIX` / `WINDOWS` / `MSDOS` / `BCC` / `LSI_C` matrix. It must be
  the first `#include` in every `.c` file because it sets feature-test macros.
- Removed the DOS far-pointer (LSI-C) and Borland-C variants of the in-memory
  file layer, and the Win16 GUI entry point (`mml_smf`, `setjmp`/`longjmp`
  error handling, `hWnd3`, the `InvalidateRect`/`UpdateWindow` stubs).
- `check_alloc_amount` was a GCC statement-expression macro with a separate
  out-of-line copy for other compilers; it is now one `static inline` function.
  `putc2`, `getc2`, `fseek2` and friends likewise.
- `isascii()` (not standard C, absent from MSVC) replaced with an explicit
  range test that correctly rejects `EOF`.
- `read()`/`write()` now loop over short transfers and `EINTR` (`<errno.h>`
  was previously used without being included, so the `EINTR` retry silently
  compiled out).
- Reading from a pipe works: `fdopen2` no longer relies on `fstat` reporting a
  size, so `mml2mid - out.mid` reads stdin correctly.

### Defects fixed along the way

These are genuine bugs found while modernising, not just tidying:

- **Stack buffer overflow in `EX`/`EE`.** `getexclusive()` wrote into
  `int exclusive[1024]` with no bounds check at six sites plus two bulk
  expansions. A long enough exclusive command overran the stack. Now bounds
  checked, reporting the new error "exclusive data too long".
- **Division by zero in random velocity.** `kr1` (or `kr n,0`) made
  `s = randvel1 * randvel2 / 2` zero, and `set_randvel()` then divided by it.
- **`text[]` overflow.** Progress and error text was accumulated with
  `strcat()` into a fixed 8 KB buffer; a track with many `S` commands could
  overrun it. All appends go through a bounded `text_cat()`, and every
  `sprintf`/`wsprintf` into `Msg[]` is now `snprintf`.
- **Title/copyright off-by-one.** The length is written as a single byte but
  the check allowed 256, which wrote a length of 0. Limit is now 255.
- **Null dereference on early error.** `free_all_macros()` is called from the
  error path and indexed `mcrstr` before `init_all_macros()` had run.
- **`realloc` leaks** on failure in `reallocmacro()` and `getLine()` (the old
  code assigned the result over the only pointer to the block).
- `put_cpres` was declared `static` and defined non-static.
- Removed the dead `dir[]` state in `setcode_I()` — written, never read.
