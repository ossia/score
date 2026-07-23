score_use_system(use_sys simpleio)
if(use_sys)
  find_library(SIMPLEIO_LIBRARY NAMES simpleio)
  find_path(SIMPLEIO_INCLUDE_DIR libsimpleio/libgpio.h)
  if(SIMPLEIO_LIBRARY AND SIMPLEIO_INCLUDE_DIR)
    add_library(simpleio INTERFACE IMPORTED GLOBAL)
    target_include_directories(simpleio SYSTEM INTERFACE "${SIMPLEIO_INCLUDE_DIR}")
    target_link_libraries(simpleio INTERFACE "${SIMPLEIO_LIBRARY}")
  endif()
endif()

find_path(LINUX_HEADERS_INCLUDE_DIR linux/gpio.h)
if(NOT TARGET simpleio AND LINUX_HEADERS_INCLUDE_DIR AND UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
add_library(simpleio STATIC
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/cplusplus.h"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/errmsg.c"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/errmsg.inc"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libadc.c"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libadc.h"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libdac.c"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libdac.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libevent.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libevent.h"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libgpio.c"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libgpio.h"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libhidraw.c"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libhidraw.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libi2c.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libi2c.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libipv4.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libipv4.h"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/liblinux.c"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/liblinux.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/liblinx.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/liblinx.h"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libpwm.c"
  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libpwm.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libserial.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libserial.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libspi.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libspi.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libstream.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libstream.h"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libwatchdog.c"
#  "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/libsimpleio/libwatchdog.h"
  )

target_include_directories(
  simpleio
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/libsimpleio/"
)
set_target_properties(simpleio PROPERTIES UNITY_BUILD 0)

endif()
