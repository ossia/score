# Find Faust2

find_path(FAUST_INCLUDE_DIR faust/dsp/llvm-dsp.h
  HINTS
  "${OSSIA_SDK}/faust/include"
  /usr/local/include
  )

# Let find_library apply platform suffixes after searching the SDK hint. Listing
# libfaust.so first makes CMake exhaust every system path for that exact filename
# before it ever considers the SDK's static libfaust.a.
set(FAUST_NAMES ${FAUST_NAMES} faust libfaust libfaust.so libfaust.dylib faust.dll)
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

      # --libs omits LLVM's platform and compression dependencies. Include
      # them in both the plug-in link and the standalone precision probes.
      execute_process(COMMAND ${LLVM_CONFIG} "--system-libs"
          OUTPUT_VARIABLE _FAUST_LLVM_SYSTEM_LIBS
          OUTPUT_STRIP_TRAILING_WHITESPACE
          COMMAND_ERROR_IS_FATAL ANY
      )
      separate_arguments(_FAUST_LLVM_SYSTEM_LIBS NATIVE_COMMAND "${_FAUST_LLVM_SYSTEM_LIBS}")
      if("-lz" IN_LIST _FAUST_LLVM_SYSTEM_LIBS)
        # The SDK's zlib need not be in the compiler's default search path.
        find_package(ZLIB REQUIRED)
        list(TRANSFORM _FAUST_LLVM_SYSTEM_LIBS REPLACE "^-lz$" "ZLIB::ZLIB")
      endif()

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
      list(APPEND FAUST_LIBRARIES ${_FAUST_LLVM_SYSTEM_LIBS})
    endif()
  endif()
else()
  set(FAUST_LIBRARIES)
  set(FAUST_INCLUDE_DIRS)
endif()

