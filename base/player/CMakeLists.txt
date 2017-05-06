cmake_minimum_required(VERSION 3.0)
project(iscore_player LANGUAGES CXX)

iscore_common_setup()
enable_testing()
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.7 COMPONENTS QuickControls2)

add_library(iscore_player
  "${CMAKE_CURRENT_SOURCE_DIR}/player_impl.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/player.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/player.cpp")
target_compile_definitions(iscore_player PRIVATE
  $<$<BOOL:${ISCORE_STATIC_PLUGINS}>:QT_STATICPLUGIN>
)
foreach(plugin ${ISCORE_LIBRARIES_LIST})
  target_link_libraries(iscore_player PUBLIC ${plugin})
endforeach()
if(ANDROID)
    list(REMOVE_ITEM ISCORE_PLUGINS_LIST iscore_plugin_midi)
endif()
foreach(plugin ${ISCORE_PLUGINS_LIST})
  target_link_libraries(iscore_player PUBLIC ${plugin})
endforeach()
target_link_libraries(iscore_player PUBLIC
  ossia
  Qt5::QuickControls2)

add_library(iscore_player_plugin SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/player_qml_plugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/player_qml_plugin.cpp"
    )
target_link_libraries(iscore_player_plugin PRIVATE iscore_player)

add_executable(player "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(player PUBLIC iscore_player)

add_executable(player_device "${CMAKE_CURRENT_SOURCE_DIR}/device_example.cpp")
target_link_libraries(player_device PUBLIC iscore_player)

add_executable(player_network
  "${CMAKE_CURRENT_SOURCE_DIR}/network_example.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/QMLPlayer.qml"
  )
target_link_libraries(player_network PUBLIC iscore_player)

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
endif()

setup_iscore_common_lib_features(iscore_player)
setup_iscore_common_lib_features(iscore_player_plugin)
setup_iscore_common_exe_features(player)
setup_iscore_common_exe_features(player_device)
setup_iscore_common_exe_features(player_network)
