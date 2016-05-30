cmake_minimum_required(VERSION 3.0)
project(iscore_remote LANGUAGES CXX)
iscore_common_setup()

set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RemoteApplication.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RemoteApplication.cpp"
    )

qt5_add_resources(REMOTE_QRCS "${CMAKE_CURRENT_SOURCE_DIR}/remote.qrc")
add_executable(iscore_remote ${SRCS} ${REMOTE_QRCS})
set_property(TARGET iscore_remote PROPERTY COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
target_link_libraries(iscore_remote PRIVATE iscore_lib_base iscore_plugin_scenario Qt5::WebSockets)

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
  target_link_libraries(iscore_remote PUBLIC ${ISCORE_PLUGINS_LIST})
endif()

setup_iscore_common_exe_features(iscore_remote)
