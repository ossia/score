cmake_minimum_required(VERSION 2.8)
project(simple_process)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)

# Files & main target
include_directories(.)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes")

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

        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/automationview.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/graphicsview.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/headerwidget.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/mainbox.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/mainwindow.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/pixmapbutton.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/pluginview.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/scenarioview.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/statedebug.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timebarwidget.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxfullview.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxheader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timebox.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxmodel.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxpresenter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxsmallview.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxstoreybar.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxstorey.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeevent.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeeventmodel.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeeventpresenter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeeventview.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/utils.hpp



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

        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/automationview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/graphicsview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/headerwidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/mainbox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/mainwindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/pixmapbutton.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/pluginview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/scenarioview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/statedebug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timebarwidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timebox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxfullview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxheader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxmodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxpresenter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxsmallview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxstoreybar.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeboxstorey.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeevent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeeventmodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeeventpresenter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/boxes/timeeventview.cpp


	${CMAKE_CURRENT_SOURCE_DIR}/process_impl/ScenarioProcessModel.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/ScenarioSettingsPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/TextChangedCommand.cpp
)

add_library(simple_process SHARED ${HEADER_FILES} ${SOURCE_FILES})
install(TARGETS simple_process LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(simple_process Qt5::Core Qt5::Widgets)
