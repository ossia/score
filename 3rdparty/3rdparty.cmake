include(3rdparty/libossia.cmake)

add_subdirectory(3rdparty/QCodeEditor)
disable_qt_plugins(QCodeEditor)

if(EMSCRIPTEN)
  return()
endif()
include(3rdparty/shmdata.cmake)
include(3rdparty/sndfile.cmake)
include(3rdparty/snappy.cmake)

