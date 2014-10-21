cmake_minimum_required(VERSION 2.8)
project(iscore_core)

# Configuration for the subprojects
set(ISCORE_PLUGINS_FOLDER "${CMAKE_BINARY_DIR}/plugins")

# Packages
find_package(Qt5Core)
find_package(Qt5Gui)
find_package(Qt5Widgets)

# Files & main target
include_directories(.)

set(HEADER_FILES
### interface ###
	${CMAKE_CURRENT_SOURCE_DIR}/interface/autoconnect/Autoconnect.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/library/LibraryElement.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/customcommand/CustomCommand.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/customcommand/MenuInterface.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/panels/Panel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/panels/BasePanel.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/processes/Process.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/processes/ProcessModel.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/settings/SettingsGroup.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/settings/SettingsGroupModel.hpp

### plugin_interface ###
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/PanelFactoryPluginInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/LibraryElementFactoryPluginInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/CustomCommandFactoryPluginInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/ProcessFactoryPluginInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/SettingsFactoryPluginInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/AutoconnectFactoryPluginInterface.hpp

### core ###
	${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/application/Application.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/model/Model.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Action.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/MenubarManager.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/CommandQueue.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Command.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/view/View.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsPresenter.hpp
)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/application/Application.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/model/Model.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/MenubarManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/CommandQueue.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Command.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/view/View.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsPresenter.cpp
)

add_executable(iscore_core ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(iscore_core Qt5::Core
								  Qt5::Gui
								  Qt5::Widgets)


install(TARGETS iscore_core DESTINATION ${CMAKE_BINARY_DIR})

# Other targets
add_subdirectory(plugins)
