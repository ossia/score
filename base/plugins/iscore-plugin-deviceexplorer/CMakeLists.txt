cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_deviceexplorer LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Test Xml)

# Files & main target
### Plugin ###
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/AddAddress.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/AddDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/LoadDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/DeviceExplorerCommandFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/RemoveNodes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove/RemoveAddress.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/ReplaceDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/UpdateAddresses.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateAddressSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateDeviceSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/AddressSettingsFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressBoolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressCharSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressImpulseSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressNoneSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressNumericSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressStringSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressTupleSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/NodeUpdateProxy.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Column.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerFilterProxyModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerMimeTypes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ExplorationWorker.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ExplorationWorkerWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ListeningManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/AddressEditDialog.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/DeviceEditDialog.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandlerFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactoryList.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/AddressAccessorEditWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/AddressEditWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/DeviceCompleter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/MessageEditDialog.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/MessageListEditor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/MessageWidget.hpp"
)


set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPluginSerialization.cpp"

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
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressImpulseSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressCharSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressStringSettingsWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPlugin.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/AddressAccessorEditWidget.cpp"

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

target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/ZeroConf/ZeroconfBrowser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/ZeroConf/ZeroconfBrowser.cpp")

target_compile_definitions(${PROJECT_NAME} PUBLIC ISCORE_ZEROCONF)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:ServusQt>)
target_include_directories(${PROJECT_NAME} PRIVATE ${OSSIA_3RDPARTY_FOLDER}/Servus)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core)
setup_iscore_plugin(iscore_plugin_deviceexplorer)
setup_iscore_tests(Tests)

