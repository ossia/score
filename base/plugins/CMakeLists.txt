# Todo : mettre output folder par défaut pour les plug-ins.
#add_subdirectory(examples)
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scenario")
	add_subdirectory(scenario)
endif()
