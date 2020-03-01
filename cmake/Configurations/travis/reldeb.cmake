if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE INTERNAL "")
endif()

if(NOT DEFINED SCORE_COTIRE)
set(SCORE_COTIRE True)
endif()
set(DEPLOYMENT_BUILD True)
set(SCORE_AUDIO_PLUGINS True CACHE INTERNAL "")

if(NOT DEFINED SCORE_ENABLE_LTO)
set(SCORE_ENABLE_LTO True)
endif()

# TODO LTO is broken on many platforms. 
# With gcc-9.2 and binutils 2.32 it seems ok so revisit when more distros use that
if(UNIX)
  if(EXISTS "/etc/centos-release")
    set(SCORE_ENABLE_LTO False)
  elseif(EXISTS "/etc/fedora-release")
    set(SCORE_ENABLE_LTO False)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(SCORE_ENABLE_LTO False)
  endif()
endif()

if(MSYS OR MINGW)
  set(SCORE_ENABLE_LTO False)
endif()

include(default-plugins)
