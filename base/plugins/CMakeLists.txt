# Todo : mettre output folder par défaut pour les plug-ins.
# add_subdirectory(examples)
add_subdirectory(plugin_settings)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scenario")
	add_subdirectory(scenario)
endif()

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
