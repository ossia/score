cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_deviceexplorer)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Test Xml)

set(DEVICEEXPLORER_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/" PARENT_SCOPE)
# Files & main target

### Plugin ###
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Plugin")
file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Singletons/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/*.hpp")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Address/DomainSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Address/AddressSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Singletons/NodeSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Singletons/SingletonProtocolList.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Singletons/DeviceExplorerInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Node/Node.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Protocol/DeviceInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Protocol/DeviceSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Protocol/ProtocolList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Protocol/ProtocolFactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Protocol/DeviceList.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Address/IOType.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Address/ClipMode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/Address/FullAddressSettings.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/QMenuView/qmenuview.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/XML/XMLDeviceLoader.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Add/AddAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Add/AddDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Add/LoadDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Update/UpdateAddressSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Update/UpdateDeviceSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Cut.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Move.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Paste.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/EditData.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Insert.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/Remove.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/ReplaceDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Commands/UpdateAddresses.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Common/AddressSettings/AddressSettingsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Common/AddressSettings/Widgets/AddressTupleSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Common/AddressSettings/Widgets/AddressBoolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Common/AddressSettings/Widgets/AddressNoneSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Common/AddressSettings/Widgets/AddressSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Common/AddressSettings/Widgets/AddressStringSettingsWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DeviceExplorerControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DeviceExplorerPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DeviceExplorerPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DocumentPlugin/DeviceDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DocumentPlugin/NodeUpdateProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/DeviceExplorerFilterProxyModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/DeviceExplorerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/ExplorationWorker.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/IOTypeDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/Widgets/DeviceEditDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/Widgets/AddressEditDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/DeviceExplorerCommandCreator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/DeviceExplorerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Panel/DeviceExplorerWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/PanelBase/DeviceExplorerPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/PanelBase/DeviceExplorerPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/PanelBase/DeviceExplorerPanelView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Widgets/DeviceExplorerMenuButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Widgets/DeviceCompleter.cpp"

#"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/ModelTest/dynamictreemodel.cpp"
#"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/ModelTest/modeltest.cpp"
#"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/ModelTest/tst_modeltest.cpp"

)
qt5_add_resources(EXPLORER_RESOURCES_RCC "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DeviceExplorer.qrc")

add_library(iscore_plugin_deviceexplorer
      ${SRCS} ${HEADERS}
      ${EXPLORER_RESOURCES_RCC})

target_link_libraries(iscore_plugin_deviceexplorer
                      Qt5::Core Qt5::Widgets Qt5::Xml Qt5::Test
                      iscore_lib_base iscore_lib_state)

iscore_cotire(iscore_plugin_deviceexplorer)
set_property(TARGET iscore_plugin_deviceexplorer APPEND
       PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

install(TARGETS iscore_plugin_deviceexplorer
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/
        COMPONENT DynamicRuntime)
