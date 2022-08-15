include(3rdparty/libossia.cmake)

add_subdirectory(3rdparty/QCodeEditor)
disable_qt_plugins(QCodeEditor)

include(3rdparty/dspfilters.cmake)
include(3rdparty/gamma.cmake)
include(3rdparty/r8brain.cmake)

if(EMSCRIPTEN)
  return()
endif()
include(3rdparty/shmdata.cmake)
include(3rdparty/sndfile.cmake)
include(3rdparty/snappy.cmake)

