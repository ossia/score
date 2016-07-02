# Find Faust2

find_path(
    FAUST_INCLUDE_DIR faust/dsp/llvm-dsp.h
    HINTS
      /opt/lib/faust/architecture/
      /usr/lib/faust/architecture/
      /usr/local/lib/faust/architecture/
      "${FAUST_INCLUDE_DIR_HINT}"
    )

set(FAUST_NAMES ${FAUST_NAMES} libfaust.so libfaust.dylib faust.dll faust libfaust)
find_library(FAUST_LIBRARY
    NAMES ${FAUST_NAMES}
    HINTS "${FAUST_LIB_DIR_HINT}")

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
else()
    set(FAUST_LIBRARIES)
    set(FAUST_INCLUDE_DIRS)
endif()
