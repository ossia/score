cmake_minimum_required(VERSION 3.0)

project(iscore_app)
# TODO strip symbols after deployment build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")

set(SRCS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
if(APPLE)
	set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/i-score.icns")
endif()
if(DEPLOYMENT_BUILD)
	add_executable(${APPNAME} WIN32 MACOSX_BUNDLE ${SRCS} ${ICON_SRC})
	if (APPLE)
		set_source_files_properties(${ICON_SRC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

		set_target_properties(${APPNAME}
				PROPERTIES
					MACOSX_BUNDLE_INFO_STRING "i-score, an interactive sequencer for the intermedia arts"
					MACOSX_BUNDLE_GUI_IDENTIFIER "org.i-score"
					MACOSX_BUNDLE_LONG_VERSION_STRING "${ISCORE_VERSION}"
					MACOSX_BUNDLE_BUNDLE_NAME "i-score"
					MACOSX_BUNDLE_SHORT_VERSION_STRING "${ISCORE_VERSION}"
					MACOSX_BUNDLE_BUNDLE_VERSION "${ISCORE_VERSION}"
					MACOSX_BUNDLE_COPYRIGHT "The i-score team"
					MACOSX_BUNDLE_ICON_FILE "i-score.icns"
					MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")
	endif ()
else()
    add_executable(${APPNAME} WIN32  ${SRCS})
endif()

target_link_libraries(${APPNAME} iscore_lib_base)
if(ISCORE_STATIC_PLUGINS)
    target_compile_definitions(${APPNAME}
                               PUBLIC ISCORE_STATIC_PLUGINS)
    target_link_libraries(${APPNAME}
                iscore_plugin_curve
                iscore_plugin_inspector
                iscore_plugin_scenario
                iscore_plugin_pluginsettings
                iscore_plugin_deviceexplorer)

    # Put these definitions as partt of the CMake interface of the plugin
    if(TARGET iscore_plugin_network)
                target_compile_definitions(${APPNAME}
                                           PUBLIC ISCORE_NETWORK)
        target_link_libraries(${APPNAME} iscore_plugin_network)
    endif()

        if(TARGET iscore_plugin_cohesion)
                target_compile_definitions(${APPNAME}
                                           PUBLIC ISCORE_COHESION)
        target_link_libraries(${APPNAME} iscore_plugin_cohesion)
    endif()

        if(TARGET iscore_plugin_ossia)
                target_compile_definitions(${APPNAME}
                                           PUBLIC ISCORE_OSSIA)
        target_link_libraries(${APPNAME} iscore_plugin_ossia)
    endif()
endif()

if(ISCORE_COTIRE)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(${APPNAME} PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()
    cotire(${APPNAME})
endif()

install(TARGETS ${APPNAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        COMPONENT Runtime)

include(IScoreDeployment)
