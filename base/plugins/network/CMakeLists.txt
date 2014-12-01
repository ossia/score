cmake_minimum_required(VERSION 2.8)
project(network_plugin)

add_subdirectory(Deps)
add_subdirectory(Repartition)

# Packages
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)
find_package(Qt5Network 5.3 REQUIRED)

find_package(Boost) 

if(NOT Boost_FOUND)
	message("The network plugin requires boost to build")
	return()
endif()

# Files & main target

include_directories(.)
include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")

include_directories("${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Repartition")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Deps/bonjour-qt/headers")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Deps/oscpack")

set(HEADERS
${CMAKE_CURRENT_SOURCE_DIR}/Deps/bonjour-qt/headers/bonjourserviceresolver.h
${CMAKE_CURRENT_SOURCE_DIR}/Deps/bonjour-qt/headers/bonjourservicebrowser.h
${CMAKE_CURRENT_SOURCE_DIR}/Deps/bonjour-qt/headers/bonjourserviceregister.h
${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/ZeroConfClient.h
${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/ZeroConfClientThread.h
${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/ZeroConfServer.h
${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/ZeroConfServerThread.h
)

set(HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkPlugin.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkCommand.hpp
#	${CMAKE_CURRENT_SOURCE_DIR}/NetworkPanel.hpp

#	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/NetworkPanelModel.hpp
#	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/NetworkPanelView.hpp
#	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/NetworkPanelPresenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsPresenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/ClientPortChangedCommand.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/ClientNameChangedCommand.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/MasterPortChangedCommand.hpp

#	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/TextChangedCommand.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitter.hpp
#	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitterClient.hpp
#	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitterMaster.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiver.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverMaster.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverClient.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/zeroconf/ZeroConfConnectDialog.hpp
)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkCommand.cpp
#	${CMAKE_CURRENT_SOURCE_DIR}/NetworkPanel.cpp

#	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/NetworkPanelModel.cpp
#	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/NetworkPanelView.cpp
#	${CMAKE_CURRENT_SOURCE_DIR}/panel_impl/NetworkPanelPresenter.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsPresenter.cpp
#	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/TextChangedCommand.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitter.cpp
#	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitterClient.cpp
#	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitterMaster.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiver.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverMaster.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/zeroconf/ZeroConfConnectDialog.cpp

)

add_library(networkplugin SHARED ${HEADER_FILES} ${SOURCE_FILES}  ${HEADERS})
install(TARGETS networkplugin LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(networkplugin
								  Qt5::Core
								  Qt5::Widgets
								  Qt5::Network
								  pthread
								  oscpack
								  dns_sd)
