# Compile one sample MML and compare the result against an expected SHA-256.
# Invoked by ctest; see the add_test() calls in CMakeLists.txt.
#
# The working directory must be the sample directory, because `#include' in an
# MML source is resolved relative to the process's current directory, not
# relative to the including file.

get_filename_component(_outdir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_outdir}")
file(REMOVE "${OUT}")

execute_process(
  COMMAND "${MML2MID}" "${STEM}.mml" "${OUT}"
  WORKING_DIRECTORY "${SAMPLEDIR}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err)

if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "mml2mid failed (exit ${_rc}) on ${STEM}.mml\n${_out}${_err}")
endif()

if(NOT EXISTS "${OUT}")
  message(FATAL_ERROR "mml2mid produced no output for ${STEM}.mml\n${_out}${_err}")
endif()

if(EXPECT_SHA STREQUAL "")
  return()  # compile-only check
endif()

file(SHA256 "${OUT}" _got)
if(NOT _got STREQUAL EXPECT_SHA)
  file(SIZE "${OUT}" _gotsz)
  message(FATAL_ERROR
    "${STEM}.mml: output does not match the recorded baseline\n"
    "  produced ${_gotsz} bytes, sha256 ${_got}\n"
    "  expected            sha256 ${EXPECT_SHA}\n"
    "  (output kept at ${OUT})")
endif()
