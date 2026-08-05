# Compile one test/trackname/*.mml and check which tracks it produced.
# Invoked by ctest; see the add_test() calls in CMakeLists.txt.
#
# Unlike the sample suite, these MMLs are written for this fork and carry no
# third-party content, so they are always available.  What they pin down is the
# track-name grammar (parallel notation, wildcards, the optional second
# character), which is easier to read as a list of track names than as a hash.
#
# The expected list lives next to the MML as a .trk file: the track names in
# output order, separated by whitespace.  mml2mid reports them on stderr as
# "trk 0A0:   96 steps".  The SMF header's track count is checked too, because
# it is written as a 16-bit field and used to be truncated to one byte.

get_filename_component(_outdir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_outdir}")
file(REMOVE "${OUT}")

execute_process(
  COMMAND "${MML2MID}" "${STEM}.mml" "${OUT}"
  WORKING_DIRECTORY "${MMLDIR}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err)

if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "mml2mid failed (exit ${_rc}) on ${STEM}.mml\n${_out}${_err}")
endif()
if(NOT EXISTS "${OUT}")
  message(FATAL_ERROR "mml2mid produced no output for ${STEM}.mml\n${_out}${_err}")
endif()

# "trk 0A0:   96 steps ..." -> 0A0
set(_got "")
string(REPLACE "\n" ";" _lines "${_out}${_err}")
foreach(_line IN LISTS _lines)
  if(_line MATCHES "^trk ([0-9A-Z]+):")
    list(APPEND _got "${CMAKE_MATCH_1}")
  endif()
endforeach()

file(READ "${MMLDIR}/${STEM}.trk" _want_raw)
string(REGEX REPLACE "[ \t\r\n]+" ";" _want "${_want_raw}")
list(FILTER _want EXCLUDE REGEX "^$")

if(NOT _got STREQUAL _want)
  string(REPLACE ";" " " _gots "${_got}")
  string(REPLACE ";" " " _wants "${_want}")
  message(FATAL_ERROR
    "${STEM}.mml: the set of tracks is not what was expected\n"
    "  produced: ${_gots}\n"
    "  expected: ${_wants}")
endif()

# SMF header: "MThd" <4-byte length> <2-byte format> <2-byte ntrks>.
# ntrks counts the tempo map on top of the tracks reported above.
list(LENGTH _got _ntrk)
math(EXPR _ntrk "${_ntrk} + 1")
file(READ "${OUT}" _hdr HEX LIMIT 12)
string(SUBSTRING "${_hdr}" 20 4 _ntrks_hex)
math(EXPR _ntrks "0x${_ntrks_hex}")
if(NOT _ntrks EQUAL _ntrk)
  message(FATAL_ERROR
    "${STEM}.mml: the SMF header says ${_ntrks} tracks, but ${_ntrk} were written")
endif()
