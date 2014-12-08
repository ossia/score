cmake_minimum_required(VERSION 3.0)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/)

if(ISCORE_STATIC_PLUGINS)
	add_definitions(-DQT_STATICPLUGIN)
endif()
# add_subdirectory(examples)

add_subdirectory(plugin_settings)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scenario2")
	add_subdirectory(scenario2)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/network")
	add_subdirectory(network)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/inspector")
	add_subdirectory(inspector)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/device_explorer")
	add_subdirectory(device_explorer)
endif()



#TODO : install : example :
#install(TARGETS the_plugino
#		ARCHIVE DESTINATION "${ISCORE_PLUGINS_FOLDER}"
#		LIBRARY DESTINATION "${ISCORE_PLUGINS_FOLDER}"
#		COMPONENT plugins)
