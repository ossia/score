cmake_minimum_required(VERSION 3.0)
project(plugin_settings_plugin)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

# Files & main target
include_directories(.)
include_directories("${CMAKE_SOURCE_DIR}")

include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")


# TODO mieux
if(APPLE)
        include_directories(/usr/local/Cellar/boost/1.56.0/include)
endif()
set(HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/PluginSettingsPlugin.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettingsModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettingsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettingsPresenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/BlacklistCommand.hpp

)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/PluginSettingsPlugin.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/PluginSettingsPresenter.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/BlacklistCommand.cpp
)

add_library(pluginsettings_plugin ${HEADER_FILES} ${SOURCE_FILES})

target_link_libraries(pluginsettings_plugin Qt5::Core Qt5::Widgets iscore_lib)
if(APPLE)
	 SET_TARGET_PROPERTIES(pluginsettings_plugin PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()
