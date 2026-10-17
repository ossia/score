# cmake -DNM=<llvm-nm> -DOUTPUT=<file.def> -DARCHIVES=<a|b|...> -DPATTERN=<regex> -P ScoreExportArchives.cmake
string(REPLACE "|" ";" ARCHIVES "${ARCHIVES}")

set(_symbols)
foreach(_archive IN LISTS ARCHIVES)
  execute_process(
    COMMAND "${NM}" --defined-only --extern-only --format=posix --quiet "${_archive}"
    OUTPUT_VARIABLE _out
    RESULT_VARIABLE _res
  )
  if(NOT _res EQUAL 0)
    message(FATAL_ERROR "${NM} failed on ${_archive}")
  endif()
  string(REPLACE "\n" ";" _out "${_out}")
  foreach(_line IN LISTS _out)
    if(NOT _line MATCHES "^([^ ]+) ([A-Za-z])")
      continue()
    endif()
    set(_sym "${CMAKE_MATCH_1}")
    set(_type "${CMAKE_MATCH_2}")
    if(NOT _sym MATCHES "${PATTERN}")
      continue()
    endif()
    if(_type MATCHES "^[Tt]$")
      list(APPEND _symbols "${_sym}")
    elseif(_type MATCHES "^[DdRrBbCc]$")
      list(APPEND _symbols "${_sym} DATA")
    endif()
  endforeach()
endforeach()

list(REMOVE_DUPLICATES _symbols)
list(SORT _symbols)
list(LENGTH _symbols _count)
message(STATUS "Exporting ${_count} symbols from ${ARCHIVES}")

list(JOIN _symbols "\n" _content)
file(WRITE "${OUTPUT}.tmp" "EXPORTS\n${_content}\n")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
