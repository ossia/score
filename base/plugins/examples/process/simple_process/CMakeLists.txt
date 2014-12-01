cmake_minimum_required(VERSION 2.8)
project(simple_process)

# Packages
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)

# Files & main target
include_directories(.)
include_directories(${ISCORE_INTERFACE_FOLDER})
include_directories(${ISCORE_INTERFACE_FOLDER}/..)

#if(APPLE)
#	set(LDFLAGS "${LDFLAGS} -undefined dynamic_lookup")
#endif()

set(HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldProcess.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldCommand.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldPanel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldCentralPanel.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldPanelModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldPanelView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldPanelPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldCentralPanelModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldCentralPanelView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldCentralPanelPresenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/process_impl/HelloWorldProcessModel.hpp
)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldProcess.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldCommand.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldPanel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldCentralPanel.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldPanelModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldPanelView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldPanelPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldCentralPanelModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldCentralPanelView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/HelloWorldCentralPanelPresenter.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/process_impl/HelloWorldProcessModel.cpp
)

#add_library(simple_process SHARED ${HEADER_FILES} ${SOURCE_FILES})
#install(TARGETS simple_process LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

#target_link_libraries(simple_process Qt5::Core Qt5::Widgets)
if(APPLE)
	 SET_TARGET_PROPERTIES(simple_process PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()
