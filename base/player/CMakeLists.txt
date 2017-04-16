cmake_minimum_required(VERSION 3.0)
project(iscore_player LANGUAGES CXX)

iscore_common_setup()
enable_testing()

# TODO strip symbols after deployment build

find_package(Qt5 5.7 COMPONENTS QuickControls2)
set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)
if(WIN32)
set(SRCS ${SRCS} "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/i-score.rc")
endif()

if(APPLE)
  set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/i-score.icns")
  set_source_files_properties(${ICON_SRC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

if(DEPLOYMENT_BUILD)
    add_executable(player WIN32 MACOSX_BUNDLE ${SRCS} ${ICON_SRC})
else()
    add_executable(player WIN32 ${SRCS})
endif()


target_link_libraries(player PUBLIC iscore_lib_base Qt5::QuickControls2)

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
  target_link_libraries(player PUBLIC ${ISCORE_PLUGINS_LIST})
endif()

setup_iscore_common_exe_features(player)

install(
  TARGETS player
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)

include(IScoreDeployment)
