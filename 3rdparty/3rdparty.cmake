include(3rdparty/libossia.cmake)

score_use_system(use_sys QCodeEditor)
if(use_sys)
  find_package(QCodeEditor GLOBAL QUIET)
endif()
if(NOT TARGET QCodeEditor)
  add_subdirectory(3rdparty/QCodeEditor)
  disable_qt_plugins(QCodeEditor)
endif()

function(disable_var VAR)
  if(${${VAR}})
    set(${VAR}_prev "${${VAR}}" CACHE "" INTERNAL FORCE)
  endif()
  set(${VAR} 0 PARENT_SCOPE)
  set(${VAR} 0 CACHE "" INTERNAL FORCE)
endfunction()

function(restore_var VAR)
  if(${${VAR}_prev})
    set(${VAR} "${${VAR}_prev}" PARENT_SCOPE)
    set(${VAR} "${${VAR}_prev}" CACHE "" INTERNAL FORCE)
  endif()
endfunction()

include(3rdparty/outcome.cmake)
include(3rdparty/quickcpplib.cmake)
include(3rdparty/llfio.cmake)
include(3rdparty/phantomstyle.cmake)
include(3rdparty/dspfilters.cmake)
include(3rdparty/eigen.cmake)
include(3rdparty/gamma.cmake)
include(3rdparty/sndfile.cmake)
include(3rdparty/xtensor.cmake)
include(3rdparty/r8brain.cmake)

if(NOT EMSCRIPTEN)
include(3rdparty/libsimpleio.cmake)
include(3rdparty/mimalloc.cmake)
include(3rdparty/sh4lt.cmake)
include(3rdparty/shmdata.cmake)
include(3rdparty/snappy.cmake)
endif()

# Not under the NOT EMSCRIPTEN guard above: 3rdparty/spz is built on every
# platform including wasm, and its CMakeLists does find_package(zstd QUIET)
# and downloads its own zstd 1.5.6 when that fails. Leaving zstd out of the
# wasm configure is what made it fail: the fetched copy is built with score's
# global unity build, and zstd's legacy/ and dictBuilder/ sources are not
# unity-safe -- every legacy version defines its own static FSE_endOfDState
# and friends, so concatenating them is a wall of redefinition errors.
include(3rdparty/zstd.cmake)
