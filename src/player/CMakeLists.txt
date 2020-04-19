project(score_player LANGUAGES CXX)

score_common_setup()
enable_testing()
set(CMAKE_AUTOMOC ON)

option(SCORE_PLAYER_PD "Build player object for PD" OFF)
option(SCORE_PLAYER_MAX "Build player object for Max" OFF)

if(SCORE_PLAYER_PD)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/score-pd")
endif()

add_library(score_player
  "${CMAKE_CURRENT_SOURCE_DIR}/player_impl.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/player.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/player.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/get_library_path.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/get_library_path.cpp"
  )
target_compile_definitions(score_player PRIVATE
  $<$<BOOL:${SCORE_STATIC_PLUGINS}>:QT_STATICPLUGIN>
)
foreach(plugin ${SCORE_LIBRARIES_LIST})
  target_link_libraries(score_player PUBLIC ${plugin})
endforeach()
if(ANDROID)
    list(REMOVE_ITEM SCORE_PLUGINS_LIST score_plugin_midi)
endif()
foreach(plugin ${SCORE_PLUGINS_LIST})
  target_link_libraries(score_player PUBLIC ${plugin})
endforeach()
target_link_libraries(score_player PUBLIC
  ossia)

add_library(score_player_plugin SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/player_qml_plugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/player_qml_plugin.cpp"
    )
target_link_libraries(score_player_plugin PRIVATE score_player)
if(ANDROID)
    target_link_libraries(score_player_plugin PRIVATE log)
endif()

add_executable(player "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(player PUBLIC score_player)

add_executable(player_device "${CMAKE_CURRENT_SOURCE_DIR}/device_example.cpp")
target_link_libraries(player_device PUBLIC score_player)

add_executable(player_network
  "${CMAKE_CURRENT_SOURCE_DIR}/network_example.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/QMLPlayer.qml"
  )
target_link_libraries(player_network PUBLIC score_player)

setup_score_common_lib_features(score_player)
setup_score_common_lib_features(score_player_plugin)
setup_score_common_exe_features(player)
setup_score_common_exe_features(player_device)
setup_score_common_exe_features(player_network)
