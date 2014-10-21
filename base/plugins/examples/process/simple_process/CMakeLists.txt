cmake_minimum_required(VERSION 2.8)
project(simple_process)

# Settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")

# Packages
find_package(Qt5Core)
find_package(Qt5Widgets)

# Files & main target
include_directories(.)

set(HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldProcess.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldCommand.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/process_impl/HelloWorldProcessModel.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettingsModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettingsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettingsPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/TextChangedCommand.hpp
)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldProcess.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldCommand.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/process_impl/HelloWorldProcessModel.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/HelloWorldSettingsPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/TextChangedCommand.cpp
)

add_library(simple_process SHARED ${HEADER_FILES} ${SOURCE_FILES})
install(TARGETS simple_process LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(simple_process Qt5::Core Qt5::Widgets)
