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
# the SHA-256 recorded in test/baseline.sha256.  See test/README.md for how
# that baseline was validated against the .mid files shipped with the samples.
# ---------------------------------------------------------------------------
enable_testing()

set(_sampledir "${CMAKE_CURRENT_SOURCE_DIR}/sample")
set(_baseline "${CMAKE_CURRENT_SOURCE_DIR}/test/baseline.sha256")

set(_hashes "")
if(EXISTS "${_baseline}")
  file(STRINGS "${_baseline}" _hashes)
endif()

file(GLOB _mml_samples "${_sampledir}/*.mml")
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
