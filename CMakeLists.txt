cmake_minimum_required(VERSION 3.16)

project(mml2mid
        VERSION 5.30
        DESCRIPTION "MML -> Standard MIDI File compiler"
        LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# gnu11 rather than c11: the program uses POSIX (open/read/isatty/strdup), and
# a strict -std=c11 makes glibc hide those declarations.  compat.h also sets
# the feature-test macros itself, so a strict build still works.
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(mml2mid
  src/mml2mid.c
  src/mmlproc.c
  src/note.c
  src/charproc.c
  src/macroexp.c
  src/dbgmap.c
  src/file.c
)

target_include_directories(mml2mid PRIVATE src)

if(MSVC)
  # /utf-8 : the sources carry Japanese comments encoded as UTF-8.
  # The _CRT_* defines silence the "use the _s variants" deprecation noise.
  target_compile_options(mml2mid PRIVATE /W3 /utf-8)
  target_compile_definitions(mml2mid PRIVATE
    _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
else()
  target_compile_options(mml2mid PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -finput-charset=UTF-8)
endif()

# ---------------------------------------------------------------------------
# WebAssembly build (Emscripten).
#
# Configure with `emcmake cmake -S . -B build-wasm'.  The result is
# build-wasm/mml2mid.mjs plus mml2mid.wasm -- an ES module exporting the
# factory `createMml2mid()'.
#
# No source change is needed: src/file.c already reads the whole input into
# memory and accumulates the output there, and everything else this program
# touches (open/read/write/close/isatty/strdup) is in Emscripten's POSIX layer,
# so the `#else' (POSIX) branch of compat.h is the right one here.
#
# The intended use is one instance per compilation, created and thrown away:
# the program keeps a lot of global state (tstep, cur_line, trknum, the macro
# tables) and calls exit() on error, so re-entering main() in a live instance
# is not safe.  Hence INVOKE_RUN=0 (we call callMain() ourselves) together with
# EXIT_RUNTIME=1 (exit() must actually tear the instance down and flush the FS).
#
# NODEFS is linked in because the VS Code extension host is Node: mounting the
# real workspace makes `#include' resolve without reimplementing it in JS.
#
# See wasm/README.md and BUILD.md.
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
  set_target_properties(mml2mid PROPERTIES SUFFIX ".mjs")
  target_link_options(mml2mid PRIVATE
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createMml2mid"
    "-sINVOKE_RUN=0"
    "-sEXIT_RUNTIME=1"
    "-sFORCE_FILESYSTEM=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sEXPORTED_RUNTIME_METHODS=['FS','callMain','NODEFS']"
    "-lnodefs.js"
    "-lidbfs.js")
endif()

include(GNUInstallDirs)
if(NOT EMSCRIPTEN)
  install(TARGETS mml2mid RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  if(NOT WIN32)
    install(FILES src/mml2mid.1
            DESTINATION ${CMAKE_INSTALL_MANDIR}/ja/man1)
  endif()
endif()

# ---------------------------------------------------------------------------
# Regression tests: compile every sample/*.mml and compare the output against
# the SHA-256 recorded in test/baseline.sha256.
#
# The sample songs are NOT bundled with this fork -- they are third-party works
# with their own copyright holders (see README).  Drop the sample/ directory
# from the original mml2mid 5.30b archive in here to enable the suite; without
# it the build is fine, there is simply nothing to test.  test/README.md has
# the details, including how the baseline was validated.
# ---------------------------------------------------------------------------
enable_testing()

# The suites all run the compiler as a plain executable, which an Emscripten
# build is not.  Verify the WASM build with wasm/smoke.mjs instead.
if(EMSCRIPTEN)
  message(STATUS "mml2mid: Emscripten build -- ctest suites disabled. "
                 "Run `node wasm/smoke.mjs build-wasm/mml2mid.mjs' instead.")
  return()
endif()

set(_sampledir "${CMAKE_CURRENT_SOURCE_DIR}/sample")
set(_baseline "${CMAKE_CURRENT_SOURCE_DIR}/test/baseline.sha256")

set(_hashes "")
if(EXISTS "${_baseline}")
  file(STRINGS "${_baseline}" _hashes)
endif()

file(GLOB _mml_samples "${_sampledir}/*.mml")

if(NOT _mml_samples)
  message(STATUS
    "mml2mid: no sample/*.mml found -- regression tests disabled. "
    "See test/README.md to enable them.")
endif()

foreach(_mml IN LISTS _mml_samples)
  get_filename_component(_stem "${_mml}" NAME_WE)

  # Look up this sample's expected hash; an empty value means "compile only".
  set(_sha "")
  foreach(_line IN LISTS _hashes)
    if(_line MATCHES "^([0-9a-f]+)[ \t]+${_stem}$")
      set(_sha "${CMAKE_MATCH_1}")
      break()
    endif()
  endforeach()

  add_test(NAME sample.${_stem}
           COMMAND ${CMAKE_COMMAND}
                   -DMML2MID=$<TARGET_FILE:mml2mid>
                   -DSAMPLEDIR=${_sampledir}
                   -DSTEM=${_stem}
                   -DEXPECT_SHA=${_sha}
                   -DOUT=${CMAKE_CURRENT_BINARY_DIR}/testout/${_stem}.mid
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunSampleTest.cmake)
endforeach()

# ---------------------------------------------------------------------------
# Bug-fix regression tests: compile every test/regress/*.mml and compare the
# output against the SHA-256 recorded in test/regress.sha256.  These pin down
# the fixes for the defects listed in org-doc/todo.txt (and, for tempo-order,
# the cases those fixes must NOT change).  Written for this fork, so they are
# always available -- unlike the sample suite above.
# ---------------------------------------------------------------------------
set(_regressdir "${CMAKE_CURRENT_SOURCE_DIR}/test/regress")
set(_regressbase "${CMAKE_CURRENT_SOURCE_DIR}/test/regress.sha256")

set(_rhashes "")
if(EXISTS "${_regressbase}")
  file(STRINGS "${_regressbase}" _rhashes)
endif()

file(GLOB _mml_regress "${_regressdir}/*.mml")

foreach(_mml IN LISTS _mml_regress)
  get_filename_component(_stem "${_mml}" NAME_WE)

  set(_sha "")
  foreach(_line IN LISTS _rhashes)
    if(_line MATCHES "^([0-9a-f]+)[ \t]+${_stem}$")
      set(_sha "${CMAKE_MATCH_1}")
      break()
    endif()
  endforeach()

  add_test(NAME regress.${_stem}
           COMMAND ${CMAKE_COMMAND}
                   -DMML2MID=$<TARGET_FILE:mml2mid>
                   -DSAMPLEDIR=${_regressdir}
                   -DSTEM=${_stem}
                   -DEXPECT_SHA=${_sha}
                   -DOUT=${CMAKE_CURRENT_BINARY_DIR}/testout/regress-${_stem}.mid
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunSampleTest.cmake)
endforeach()

# ---------------------------------------------------------------------------
# Track-name tests: compile every test/trackname/*.mml and check the list of
# tracks it produced against the .trk file beside it.  These MMLs are written
# for this fork, so unlike the sample suite they are always available.
# ---------------------------------------------------------------------------
set(_trknamedir "${CMAKE_CURRENT_SOURCE_DIR}/test/trackname")
file(GLOB _mml_tracknames "${_trknamedir}/*.mml")

foreach(_mml IN LISTS _mml_tracknames)
  get_filename_component(_stem "${_mml}" NAME_WE)

  add_test(NAME trackname.${_stem}
           COMMAND ${CMAKE_COMMAND}
                   -DMML2MID=$<TARGET_FILE:mml2mid>
                   -DMMLDIR=${_trknamedir}
                   -DSTEM=${_stem}
                   -DOUT=${CMAKE_CURRENT_BINARY_DIR}/testout/trackname-${_stem}.mid
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTrackNameTest.cmake)
endforeach()

# ---------------------------------------------------------------------------
# Debug-map tests: compile every test/dbgmap/*.mml with and without `-g2' and
# check (a) that the SMF is byte-identical either way -- the compatibility
# guarantee the option rests on -- and (b) that the map matches the recorded
# .expected file.  See doc/DEBUGMAP.md.
# ---------------------------------------------------------------------------
set(_dbgmapdir "${CMAKE_CURRENT_SOURCE_DIR}/test/dbgmap")
file(GLOB _mml_dbgmap "${_dbgmapdir}/*.mml")

foreach(_mml IN LISTS _mml_dbgmap)
  get_filename_component(_stem "${_mml}" NAME_WE)

  add_test(NAME dbgmap.${_stem}
           COMMAND ${CMAKE_COMMAND}
                   -DMML2MID=$<TARGET_FILE:mml2mid>
                   -DMMLDIR=${_dbgmapdir}
                   -DSTEM=${_stem}
                   -DEXPECTED=${_dbgmapdir}/${_stem}.expected
                   -DOUTDIR=${CMAKE_CURRENT_BINARY_DIR}/testout/dbgmap
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunDebugMapTest.cmake)
endforeach()

# ---------------------------------------------------------------------------
# Diagnostics tests: compile every test/diag/*.mml with `--diag=json' and check
# the JSON objects it writes to stderr against the recorded .expected file.
# ---------------------------------------------------------------------------
set(_diagdir "${CMAKE_CURRENT_SOURCE_DIR}/test/diag")
file(GLOB _mml_diag "${_diagdir}/*.mml")

foreach(_mml IN LISTS _mml_diag)
  get_filename_component(_stem "${_mml}" NAME_WE)

  add_test(NAME diag.${_stem}
           COMMAND ${CMAKE_COMMAND}
                   -DMML2MID=$<TARGET_FILE:mml2mid>
                   -DMMLDIR=${_diagdir}
                   -DSTEM=${_stem}
                   -DEXPECTED=${_diagdir}/${_stem}.expected
                   -DOUTDIR=${CMAKE_CURRENT_BINARY_DIR}/testout/diag
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunDiagTest.cmake)
endforeach()
