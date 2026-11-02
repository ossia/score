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

  if("${FAUST_LIBRARY}" MATCHES ".*\\.a$")
    # Imported targets carry LLVM's platform and compression dependencies into
    # both the plug-in link and the standalone precision probes.
    find_package(LLVM REQUIRED CONFIG)
    if(LLVM_LINK_LLVM_DYLIB AND TARGET LLVM)
      list(APPEND FAUST_LIBRARIES LLVM)
    else()
      set(_FAUST_LLVM_LIBRARIES ${LLVM_AVAILABLE_LIBS})
      list(REMOVE_ITEM _FAUST_LLVM_LIBRARIES LTO Remarks)
      list(APPEND FAUST_LIBRARIES ${_FAUST_LLVM_LIBRARIES})
    endif()
  endif()
else()
  set(FAUST_LIBRARIES)
  set(FAUST_INCLUDE_DIRS)
endif()

