cmake_minimum_required(VERSION 3.0)

project(iscore_app)
set(APPNAME "i-score")
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Network Xml)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")


if(ISCORE_STATIC_PLUGINS)
	add_definitions(-DQT_STATICPLUGIN)
endif()

if(DEPLOYMENT_BUILD)
	add_executable(${APPNAME} WIN32 MACOSX_BUNDLE main.cpp)
else()
	add_executable(${APPNAME} WIN32 main.cpp)
endif()

target_link_libraries(${APPNAME} iscore_lib)
if(ISCORE_STATIC_PLUGINS)
	add_definitions(-DISCORE_STATIC_PLUGINS)
	target_link_libraries(${APPNAME}
		CurvePlugin
		inspector_plugin 
		networkplugin 
		scenario_process 
		pluginsettings_plugin
		device_explorer_plugin)
else()
	if(APPLE AND DEPLOYMENT_BUILD)
		# Copy into the app bundle
		add_custom_command(TARGET ${APPNAME} POST_BUILD
						   COMMAND mkdir -p ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)
		add_custom_command(TARGET ${APPNAME} POST_BUILD
						   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:CurvePlugin> ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)
		add_custom_command(TARGET ${APPNAME} POST_BUILD
						   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:inspector_plugin> ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)
		add_custom_command(TARGET ${APPNAME} POST_BUILD
						   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:networkplugin> ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)
		add_custom_command(TARGET ${APPNAME} POST_BUILD
						   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:scenario_process> ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)
		add_custom_command(TARGET ${APPNAME} POST_BUILD
						   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pluginsettings_plugin> ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)
		add_custom_command(TARGET ${APPNAME} POST_BUILD
						   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:device_explorer_plugin> ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)

		# TODO it's not a plugin. It should be copied to the MacOS folder instead.
		#add_custom_command(TARGET ${APPNAME} POST_BUILD
		#				   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:process_interface> ${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/)

	endif()
endif()

install(TARGETS ${APPNAME}
		BUNDLE DESTINATION .
		RUNTIME DESTINATION bin
		COMPONENT Runtime)



if(APPLE AND DEPLOYMENT_BUILD)
	# Remember to set CMAKE_INSTALL_PREFIX on the CMake command line.
	include(DeployQt5)

	if(ISCORE_STATIC_PLUGINS) # For now we only take this branch
		install_qt5_executable(${APPNAME}.app "" "" "${CMAKE_BINARY_DIR}/base/lib")
	else()
		# TODO fix this.
		set(ISCORE_PLUGINS "device_explorer_plugin;inspector_plugin;scenario_process;CurvePlugin;iscore_cohesion;networkplugin;pluginsettings_plugin")

		foreach(plugin ${ISCORE_PLUGINS})
			list(APPEND ISCORE_BUNDLE_INSTALLED_PLUGINS ${APPNAME}.app/Contents/MacOS/plugins/lib${plugin}.dylib)
		endforeach()

		install_qt5_executable(${APPNAME}.app
							   ""
							   "${ISCORE_BUNDLE_INSTALLED_PLUGINS}"
							   "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/plugins;${CMAKE_BINARY_DIR}/base/lib;${CMAKE_BINARY_DIR}/${APPNAME}.app/Contents/MacOS/plugins/")
	endif()
endif()
