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
    if (NOT "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "${CMAKE_HOST_SYSTEM_PROCESSOR}")
      find_package(LLVM REQUIRED CONFIG)
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
        find_package(LLVM REQUIRED CONFIG)
        set(LLVM_LIBS "${LLVM_AVAILABLE_LIBS}")
        list(REMOVE_ITEM LLVM_LIBS LTO Remarks)
        set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${LLVM_LIBS})
        return()
      endif()

      execute_process(COMMAND ${LLVM_CONFIG} "--includedir"
          OUTPUT_VARIABLE LLVM_DIR
          ERROR_VARIABLE LLVM_CONFIG_STDERR
          RESULT_VARIABLE LLVM_ERROR
      )
      if(NOT "${LLVM_ERROR}" EQUAL 0)
        message(FATAL_ERROR "Could not run llvm-config: \n '${LLVM_DIR}'\n => '${LLVM_CONFIG_STDERR}'\n => ${LLVM_ERROR}")
      endif()
      execute_process(COMMAND ${LLVM_CONFIG} "--libs" OUTPUT_VARIABLE LLVM_LIBS)
      string(STRIP "${LLVM_LIBS}" LLVM_LIBS)

      execute_process(COMMAND ${LLVM_CONFIG} "--version" OUTPUT_VARIABLE LLVM_VERSION)
      string(STRIP "${LLVM_VERSION}" LLVM_VERSION)

      execute_process(COMMAND ${LLVM_CONFIG} "--ldflags" OUTPUT_VARIABLE LLVM_LDFLAGS)
      string(STRIP "${LLVM_LDFLAGS}" LLVM_LDFLAGS)

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
        set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${CMAKE_DL_LIBS}  z ${LLVM_LDFLAGS} ${LLVM_LIBS} )
      elseif(APPLE)
        string(REGEX REPLACE " " ";" LLVM_LIBS ${LLVM_LIBS})
        set(FAUST_LIBRARIES ${FAUST_LIBRARIES} ${LLVM_LDFLAGS} ${LLVM_LIBS})
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

