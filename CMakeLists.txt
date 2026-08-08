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

include(GNUInstallDirs)
install(TARGETS mml2mid RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(NOT WIN32)
  install(FILES src/mml2mid.1
          DESTINATION ${CMAKE_INSTALL_MANDIR}/ja/man1)
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
