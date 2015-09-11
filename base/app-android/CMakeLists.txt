cmake_minimum_required(VERSION 3.0)
 
project(iscore_app_android)

find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets Xml Network Test)
#~ 
#~ set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/base/app-android/libs/${ARM_TARGET}/)
#~ SET(CMAKE_SKIP_BUILD_RPATH FALSE)
#~ SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
#~ SET(CMAKE_INSTALL_RPATH ".")
#~ SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")
 
add_library(iscore_app_android SHARED main.cpp)
set_target_properties(iscore_app_android PROPERTIES VERSION ${ISCORE_VERSION})
 
target_link_libraries(iscore_app_android iscore_lib)
target_link_libraries(iscore_app_android Qt5::Core Qt5::Widgets)

	add_definitions(-DISCORE_STATIC_PLUGINS)
	target_link_libraries(iscore_app_android 
		inspector_plugin 
		scenario_process 
		pluginsettings_plugin 
		device_explorer_plugin)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake/Android/qt-android-cmake/AddQtAndroidApk.cmake)
add_qt_android_apk(iscore_apk iscore_app_android
                   PACKAGE_NAME "net.iscore"
                   INSTALL)
