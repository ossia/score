cmake_minimum_required(VERSION 3.0)
project(network_plugin)

if(ANDROID OR WIN32)
	message("TODO FIXME ON ANDROID AND WINDOWS")
	return()
endif()
add_subdirectory(Deps)
add_subdirectory(Repartition)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Network)


# Files & main target
include_directories(.)

include_directories("${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Repartition")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Deps/bonjour-qt/headers")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Deps/oscpack")

set(SRC_Serialization
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkSerialization.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkSerialization.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkMessage.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkMessage.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkSocket.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkSocket.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkServer.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkServer.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/MessageMapper.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/MessageMapper.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/MessageValidator.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Serialization/MessageValidator.cpp
)
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
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkDocumentPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkCommand.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsPresenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/ClientPortChangedCommand.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/ClientNameChangedCommand.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/commands/MasterPortChangedCommand.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiver.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverMaster.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverClient.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/zeroconf/ZeroConfConnectDialog.hpp
)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkDocumentPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/NetworkCommand.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsPresenter.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionEmitter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiver.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteActionReceiverMaster.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/zeroconf/ZeroConfConnectDialog.cpp

)

add_library(networkplugin ${HEADER_FILES} ${SOURCE_FILES} ${SRC_Serialization} ${HEADERS})

target_link_libraries(networkplugin
									iscore_lib
									Qt5::Core
									Qt5::Widgets
									Qt5::Network
									pthread
									oscpack)

if(NOT APPLE)
	target_link_libraries(networkplugin dns_sd)
endif()

install(TARGETS networkplugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
