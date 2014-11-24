cmake_minimum_required(VERSION 2.8)
project(plugin_settings_plugin)

# Packages
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)

# Files & main target
include_directories(.)
include_directories("${CMAKE_SOURCE_DIR}")

include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")


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

add_library(pluginsettings_plugin SHARED ${HEADER_FILES} ${SOURCE_FILES})
install(TARGETS pluginsettings_plugin LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(pluginsettings_plugin Qt5::Core Qt5::Widgets)
if(APPLE)
     SET_TARGET_PROPERTIES(pluginsettings_plugin PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()
