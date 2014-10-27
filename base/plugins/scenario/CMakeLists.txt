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
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioPanel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCentralPanel.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioPanelModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioPanelView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioPanelPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelPresenter.hpp

                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/automationview.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/graphicsview.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/headerwidget.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/mainwindow.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/pixmapbutton.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/pluginview.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/scenarioview.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/statedebug.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/timebarwidget.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxfullview.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxheader.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timebox.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxmodel.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxpresenter.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxsmallview.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxstoreybar.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxstorey.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeevent.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeeventmodel.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeeventpresenter.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeeventview.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/utils.hpp



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
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioPanel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScenarioCentralPanel.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioPanelModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioPanelView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioPanelPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/ScenarioCentralPanelPresenter.cpp

                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/automationview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/graphicsview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/headerwidget.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/mainwindow.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/pixmapbutton.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/pluginview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/scenarioview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/statedebug.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/interface/timebarwidget.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timebox.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxfullview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxheader.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxmodel.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxpresenter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxsmallview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxstoreybar.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeBox/timeboxstorey.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeevent.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeeventmodel.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeeventpresenter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/timeEvent/timeeventview.cpp


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
