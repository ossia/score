cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_network)

if(ANDROID)
    message("TODO FIXME ON ANDROID")
    return()
endif()

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Network Xml Test)
find_package(KF5DNSSD NO_MODULE)

set(CMAKE_AUTOMOC ON)

# Files & main target
include_directories(.)
include_directories("${SCENARIO_INTERFACE_FOLDER}")
include_directories("${ISCORE_ROOT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Repartition")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/GroupSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/GroupManagerSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/GroupMetadataSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugins/PlaceholderNetworkPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugins/NetworkDocumentPluginSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Commands/AddClientToGroup.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Commands/RemoveClientFromGroup.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Commands/CreateGroup.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Commands/ChangeGroup.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Group.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/GroupManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/GroupMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/GroupPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/GroupPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/GroupPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/Widgets/GroupHeaderItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/Widgets/GroupListWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/Widgets/GroupTableCheckbox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/Widgets/GroupWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/Widgets/SessionHeaderItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/Widgets/GroupTableWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/Panel/GroupPanelView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DistributedScenario/GroupMetadataWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugins/NetworkClientDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugins/NetworkDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugins/NetworkMasterDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/NetworkControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/NetworkPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/ClientSession.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/ClientSessionBuilder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/MasterSession.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/Session.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Repartition/session/RemoteClientBuilder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Serialization/MessageMapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Serialization/MessageValidator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkMessage.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkServer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Serialization/NetworkSocket.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Tests/Repartition/client/main.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Tests/Repartition/server/main.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/NetworkSettingsView.cpp"
)

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

#Remove zeroconf header from the glob
foreach(item ${HDRS})
  if(${item} MATCHES "Zeroconf")
    list(REMOVE_ITEM HDRS ${item})
  endif()
endforeach()

if(${KF5DNSSD_FOUND})
    message("Will use ZeroConf")
    file(GLOB_RECURSE ZEROCONF_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/Zeroconf/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Zeroconf/*.hpp")

    if(WIN32)
        include_directories(/f/kdnssd/usr/include)
        include_directories("f:/kdnssd/usr/include")
    endif()
endif()

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${ZEROCONF_SRCS})

target_link_libraries(${PROJECT_NAME}
                                    iscore_lib_base
                                    iscore_plugin_scenario # TODO :(
                                    Qt5::Core
                                    Qt5::Widgets
                                    Qt5::Network)

if(${KF5DNSSD_FOUND})
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_ZEROCONF)
    target_link_libraries(${PROJECT_NAME} KF5::DNSSD)
endif()

if(NOT APPLE AND NOT WIN32)
    target_link_libraries(${PROJECT_NAME} dns_sd)
endif()

if(ISCORE_COTIRE)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(${PROJECT_NAME} PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()
    cotire(${PROJECT_NAME})
endif()

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/
        COMPONENT DynamicRuntime)
