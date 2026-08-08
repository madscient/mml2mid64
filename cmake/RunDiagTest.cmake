# Compile one MML with `--diag=json' and check the JSON diagnostics against the
# recorded .expected file.  Only the JSON lines are compared -- the human
# readable messages around them are not part of the contract.
#
# Invoked by ctest; see the add_test() calls in CMakeLists.txt.

set(_work "${OUTDIR}/${STEM}")
file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}")

file(GLOB _srcs "${MMLDIR}/*.mml")
file(COPY ${_srcs} DESTINATION "${_work}")

execute_process(
  COMMAND "${MML2MID}" "--diag=json" "${STEM}.mml" "out.mid"
  WORKING_DIRECTORY "${_work}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err)

# Keep only the JSON objects.  mml2mid writes them to stderr, one per line,
# interleaved with the ordinary messages.
string(REPLACE "\r\n" "\n" _err "${_err}")
string(REPLACE "\n" ";" _lines "${_err}")
set(_got "")
foreach(_line IN LISTS _lines)
  if(_line MATCHES "^{.*}$")
    set(_got "${_got}${_line}\n")
  endif()
endforeach()

if(NOT EXISTS "${EXPECTED}")
  message(FATAL_ERROR
    "${STEM}.mml: no expected diagnostics recorded.  Produced:\n${_got}")
endif()

file(READ "${EXPECTED}" _want)
string(REPLACE "\r\n" "\n" _want "${_want}")

if(NOT _got STREQUAL _want)
  message(FATAL_ERROR
    "${STEM}.mml: diagnostics do not match ${EXPECTED}\n"
    "--- expected ---\n${_want}"
    "--- produced ---\n${_got}"
    "--- full stderr ---\n${_err}")
endif()
