cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_deviceexplorer LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Test Xml)

# Files & main target
### Plugin ###
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/*.hpp")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/AddAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/AddDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/LoadDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove/RemoveAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateAddressSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateDeviceSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/DeviceExplorerCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/ReplaceDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/UpdateAddresses.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/AddressSettingsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressTupleSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressBoolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressNoneSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressCharSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressStringSettingsWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DeviceExplorerPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPluginFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/NodeUpdateProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerFilterProxyModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ExplorationWorker.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/DeviceEditDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/AddressEditDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ListeningManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/DeviceCompleter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/MessageWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/MessageListEditor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/MessageEditDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/AddressEditWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactoryList.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandlerFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_deviceexplorer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorerApplicationPlugin.cpp"

#"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ModelTest/dynamictreemodel.cpp"
#"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ModelTest/modeltest.cpp"
#"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ModelTest/tst_modeltest.cpp"

)
qt5_add_resources(EXPLORER_RESOURCES_RCC "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DeviceExplorer.qrc")

add_library(${PROJECT_NAME}
      ${SRCS} ${HDRS}
      ${EXPLORER_RESOURCES_RCC})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(iscore_plugin_deviceexplorer
        PUBLIC
                iscore_lib_device
        PRIVATE
                QProgressIndicator)

setup_iscore_plugin(iscore_plugin_deviceexplorer)
setup_iscore_tests(Tests)

