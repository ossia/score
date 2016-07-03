# - Try to find Portaudio
# Once done this will define
#
#  PORTAUDIO_FOUND - system has Portaudio
#  PORTAUDIO_INCLUDE_DIRS - the Portaudio include directory
#  PORTAUDIO_LIBRARIES - Link these to use Portaudio
#  PORTAUDIO_DEFINITIONS - Compiler switches required for using Portaudio
#  PORTAUDIO_VERSION - Portaudio version
#
#  Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#
# Redistribution and use is allowed according to the terms of the New BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if (NOT WIN32)
 include(FindPkgConfig)
 pkg_check_modules(PORTAUDIO2 portaudio-2.0)
endif (NOT WIN32)

if (PORTAUDIO2_FOUND)
  set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO2_INCLUDE_DIRS})
  message("${PORTAUDIO2_INCLUDE_DIRS}")
  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(PORTAUDIO_LIBRARIES "${PORTAUDIO2_LIBRARY_DIRS}/lib${PORTAUDIO2_LIBRARIES}.dylib")
  else ()
    set(PORTAUDIO_LIBRARIES ${PORTAUDIO2_LIBRARIES})
  endif ()
  set(PORTAUDIO_VERSION 19)
endif()

if(NOT PORTAUDIO2_FOUND OR "${PORTAUDIO_INCLUDE_DIRS}" MATCHES "")
  find_path(PORTAUDIO_INCLUDE_DIR
    NAMES
      portaudio.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
      "${PORTAUDIO_INCLUDE_DIR_HINT}"
  )

  find_library(PORTAUDIO_LIBRARY
    NAMES
      portaudio
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      "${PORTAUDIO_LIB_DIR_HINT}"
  )

  set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
  set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY})
  set(PORTAUDIO_VERSION 19)
endif ()


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Portaudio DEFAULT_MSG
  PORTAUDIO_LIBRARIES PORTAUDIO_INCLUDE_DIRS)
