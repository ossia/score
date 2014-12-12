cmake_minimum_required(VERSION 2.8)
project(Repartition)

set(HDRS
${CMAKE_CURRENT_SOURCE_DIR}/client/Client.h
${CMAKE_CURRENT_SOURCE_DIR}/client/ClientManager.h
${CMAKE_CURRENT_SOURCE_DIR}/client/LocalClient.h
${CMAKE_CURRENT_SOURCE_DIR}/client/LocalMaster.h
${CMAKE_CURRENT_SOURCE_DIR}/client/RemoteClient.h
${CMAKE_CURRENT_SOURCE_DIR}/client/RemoteMaster.h
${CMAKE_CURRENT_SOURCE_DIR}/client/RemoteSender.h
${CMAKE_CURRENT_SOURCE_DIR}/group/BackupGroup.h
${CMAKE_CURRENT_SOURCE_DIR}/group/GroupManager.h
${CMAKE_CURRENT_SOURCE_DIR}/group/Group.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/oscreceiver.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/oscsender.h
${CMAKE_CURRENT_SOURCE_DIR}/osc/oscmessagegenerator.h
${CMAKE_CURRENT_SOURCE_DIR}/permission/full/Permission.h
${CMAKE_CURRENT_SOURCE_DIR}/permission/full/PermissionManager.h
${CMAKE_CURRENT_SOURCE_DIR}/permission/view/PermissionView.h
${CMAKE_CURRENT_SOURCE_DIR}/permission/view/PermissionViewManager.h
${CMAKE_CURRENT_SOURCE_DIR}/permission/PermissionBase.h
${CMAKE_CURRENT_SOURCE_DIR}/properties/GenerateUniqueProperty.h
${CMAKE_CURRENT_SOURCE_DIR}/properties/hasId.h
${CMAKE_CURRENT_SOURCE_DIR}/properties/hasName.h
${CMAKE_CURRENT_SOURCE_DIR}/session/MasterSession.h
${CMAKE_CURRENT_SOURCE_DIR}/session/Session.h
${CMAKE_CURRENT_SOURCE_DIR}/session/ZeroConfClientThread.h
${CMAKE_CURRENT_SOURCE_DIR}/session/ZeroConfServerThread.h
${CMAKE_CURRENT_SOURCE_DIR}/session/ClientSession.h
${CMAKE_CURRENT_SOURCE_DIR}/session/ClientSessionBuilder.h
${CMAKE_CURRENT_SOURCE_DIR}/session/ZeroConfServer.h
${CMAKE_CURRENT_SOURCE_DIR}/session/ZeroConfClient.h
${CMAKE_CURRENT_SOURCE_DIR}/session/ConnectionData.hpp
${CMAKE_CURRENT_SOURCE_DIR}/synchronisation_policy/SynchronisationPolicy.h
${CMAKE_CURRENT_SOURCE_DIR}/DistributedProcess.h
${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario.h
${CMAKE_CURRENT_SOURCE_DIR}/Iterable.h)

add_custom_target(${PROJECT_NAME} SOURCES ${HDRS})
