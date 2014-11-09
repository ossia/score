cmake_minimum_required(VERSION 2.8)
project(simple_process)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)

# Files & main target
include_directories(.)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface")

set(HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioProcess.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCommand.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCommandImpl.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCentralPanel.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelPresenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/AutomationView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/GraphicsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/HeaderWidget.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/MainWindow.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/PixmapButton.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/PluginView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioContainer.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/StateDebug.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/TimeBarWidget.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxFullView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxHeader.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBox.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxSmallView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxStoreyBar.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxStorey.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEvent.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEventModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEventPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEventView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/Utils.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/process_impl/ScenarioProcessModel.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/TextChangedCommand.hpp
)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioProcess.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCommand.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCommandImpl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCentralPanel.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelPresenter.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/AutomationView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/GraphicsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/HeaderWidget.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/MainWindow.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/PixmapButton.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/PluginView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioContainer/ScenarioContainer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/StateDebug.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/TimeBarWidget.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBox.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxFullView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxHeader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxSmallView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxStoreyBar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeBox/TimeBoxStorey.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEvent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEventModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEventPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/TimeEvent/TimeEventView.cpp


	${CMAKE_CURRENT_SOURCE_DIR}/process_impl/ScenarioProcessModel.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/TextChangedCommand.cpp
)

add_library(example_process SHARED ${HEADER_FILES} ${SOURCE_FILES})
install(TARGETS example_process LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(example_process Qt5::Core Qt5::Widgets)
if(APPLE)
	 SET_TARGET_PROPERTIES(example_process PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()
