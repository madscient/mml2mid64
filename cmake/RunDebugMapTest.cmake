# Compile one MML twice -- with and without `-g2' -- and check that
#   (1) the SMF is byte-for-byte identical either way, and
#   (2) the debug map matches the recorded .expected file.
#
# Invoked by ctest; see the add_test() calls in CMakeLists.txt.
#
# The sources are copied into a scratch directory and compiled there with bare
# filenames.  That keeps the "files" array in the map free of absolute paths,
# and it is also what `#include' needs: an include is resolved relative to the
# process's current directory, not relative to the including file.

set(_work "${OUTDIR}/${STEM}")
file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}")

file(GLOB _srcs "${MMLDIR}/*.mml")
file(COPY ${_srcs} DESTINATION "${_work}")
if(EXISTS "${MMLDIR}/inc")
  file(COPY "${MMLDIR}/inc" DESTINATION "${_work}")
endif()

# --- 1. without -g -------------------------------------------------------
execute_process(
  COMMAND "${MML2MID}" "${STEM}.mml" "plain.mid"
  WORKING_DIRECTORY "${_work}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "mml2mid failed (exit ${_rc}) on ${STEM}.mml\n${_out}${_err}")
endif()

# --- 2. with -g2 ---------------------------------------------------------
execute_process(
  COMMAND "${MML2MID}" "-g2" "${STEM}.mml" "mapped.mid"
  WORKING_DIRECTORY "${_work}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "mml2mid -g2 failed (exit ${_rc}) on ${STEM}.mml\n${_out}${_err}")
endif()

# --- 3. -g must not change a single byte of the SMF ----------------------
file(SHA256 "${_work}/plain.mid" _sha_plain)
file(SHA256 "${_work}/mapped.mid" _sha_mapped)
if(NOT _sha_plain STREQUAL _sha_mapped)
  message(FATAL_ERROR
    "${STEM}.mml: -g2 changed the generated SMF\n"
    "  without -g: ${_sha_plain}\n"
    "  with -g2:   ${_sha_mapped}\n"
    "  (kept in ${_work})")
endif()

# --- 4. the map itself ---------------------------------------------------
set(_map "${_work}/mapped.mmlmap.json")
if(NOT EXISTS "${_map}")
  message(FATAL_ERROR "${STEM}.mml: -g2 produced no debug map\n${_out}${_err}")
endif()

if(NOT EXISTS "${EXPECTED}")
  message(FATAL_ERROR
    "${STEM}.mml: no expected map recorded.\n"
    "  To record it, review and copy:\n"
    "    ${_map}\n"
    "  to ${EXPECTED}")
endif()

file(READ "${_map}" _got)
file(READ "${EXPECTED}" _want)
# Compare ignoring line-ending differences: the .expected files are stored
# with LF, but the C code writes them through a text-mode FILE*.
string(REPLACE "\r\n" "\n" _got "${_got}")
string(REPLACE "\r\n" "\n" _want "${_want}")

if(NOT _got STREQUAL _want)
  message(FATAL_ERROR
    "${STEM}.mml: debug map does not match ${EXPECTED}\n"
    "--- expected ---\n${_want}"
    "--- produced ---\n${_got}"
    "  (produced map kept at ${_map})")
endif()
