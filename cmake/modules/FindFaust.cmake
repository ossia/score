# Find Faust2

find_path(FAUST_INCLUDE_DIR faust/dsp/llvm-dsp.h
  HINTS
  "${OSSIA_SDK}/faust/include"
  /usr/local/include
  )

set(FAUST_NAMES ${FAUST_NAMES} libfaust.so libfaust.dylib faust.dll faust libfaust)
find_library(FAUST_LIBRARY
  NAMES ${FAUST_NAMES}
  HINTS
  "${OSSIA_SDK}/faust/lib"
  /usr/local/lib
  )

if(FAUST_INCLUDE_DIR AND FAUST_LIBRARY)
  set(FAUST_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Faust DEFAULT_MSG
  FAUST_LIBRARY FAUST_INCLUDE_DIR)

if(FAUST_FOUND)
  set(FAUST_LIBRARIES ${FAUST_LIBRARY})
  set(FAUST_INCLUDE_DIRS ${FAUST_INCLUDE_DIR})

  if("${FAUST_LIBRARY}" MATCHES ".*\.a$")
    # This is a static build of faust, hence
    # we have to add all the LLVM flags...
    find_package(LLVM REQUIRED CONFIG)
    if (NOT "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "${CMAKE_HOST_SYSTEM_PROCESSOR}")
      # llvm_map_components_to_libnames(LLVM_LIBS all)
      set(LLVM_LIBS "${LLVM_AVAILABLE_LIBS}")
      list(REMOVE_ITEM LLVM_LIBS LTO Remarks)
      set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${LLVM_LIBS})
    else()
      find_program(LLVM_CONFIG llvm-config
        HINTS
        "${OSSIA_SDK}/llvm-libs/bin"
        "${OSSIA_SDK}/llvm/bin"
        )

      if(NOT LLVM_CONFIG)
        message("Using a static Faust library requires LLVM tooling to be present in the path")
        unset(FAUST_LIBRARIES)
        unset(FAUST_INCLUDE_DIRS)
        unset(FAUST_FOUND)
        return()
      endif()

      exec_program(${LLVM_CONFIG} ARGS "--includedir" OUTPUT_VARIABLE LLVM_DIR RETURN_VALUE LLVM_ERROR)
      if(NOT "${LLVM_ERROR}" EQUAL 0)
        message(FATAL_ERROR "Could not run llvm-config: ${LLVM_DIR}")
      endif()
      exec_program(${LLVM_CONFIG} ARGS "--libs" OUTPUT_VARIABLE LLVM_LIBS)
      exec_program(${LLVM_CONFIG} ARGS "--version" OUTPUT_VARIABLE LLVM_VERSION)
      exec_program(${LLVM_CONFIG} ARGS "--ldflags" OUTPUT_VARIABLE LLVM_LDFLAGS)

      file(TO_CMAKE_PATH "${LLVM_LDFLAGS}" LLVM_LDFLAGS)
      file(TO_CMAKE_PATH "${LLVM_DIR}" LLVM_DIR)

      file(TO_CMAKE_PATH "${LLVM_DIR}" LLVM_DIR)
      file(TO_CMAKE_PATH "${LLVM_LDFLAGS}" LLVM_LDFLAGS)

      if(MSYS)
        string(REGEX REPLACE "([a-zA-Z]):" "/\\1" LLVM_LDFLAGS "${LLVM_LDFLAGS}")
        string(REPLACE "\\" "/" LLVM_LDFLAGS "${LLVM_LDFLAGS}")
      endif()

      set(LLVM_VERSION LLVM_${LLVM_VERSION_MAJOR}${LLVM_VERSION_MINOR})

      if(MINGW)
        set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${CMAKE_DL_LIBS} ${LLVM_LDFLAGS} ${LLVM_LIBS} )
      elseif(NOT MSVC AND NOT APPLE)
        set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${CMAKE_DL_LIBS} curses z ${LLVM_LDFLAGS} ${LLVM_LIBS} )
      elseif(APPLE)
        string(REGEX REPLACE " " ";" LLVM_LIBS ${LLVM_LIBS})
        set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${LLVM_LDFLAGS} ${LLVM_LIBS} curses z)
      else()
        string(REGEX REPLACE " " ";" LLVM_LIBS ${LLVM_LIBS})
        set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${LLVM_LIBS})
      endif()
    endif()
  endif()
else()
  set(FAUST_LIBRARIES)
  set(FAUST_INCLUDE_DIRS)
endif()

