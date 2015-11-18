cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_deviceexplorer)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Xml)

# Files & main target
### Plugin ###
file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/Device/*.hpp")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/DomainSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/AddressSettingsSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/IOType.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/ClipMode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/FullAddressSettings.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Device/Node/DeviceNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Node/DeviceNodeSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/DeviceInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/DeviceSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/ProtocolFactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/DeviceList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/ProtocolSettingsWidget.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Device/ItemModels/NodeBasedItemModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/ItemModels/NodeDisplayMethods.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Device/QMenuView/qmenuview.cpp" # TODO move in 3rdparty
"${CMAKE_CURRENT_SOURCE_DIR}/Device/XML/XMLDeviceLoader.cpp"
)

add_library(iscore_lib_device ${SRCS} ${HEADERS})

target_link_libraries(iscore_lib_device
                      Qt5::Core Qt5::Widgets Qt5::Xml
                      iscore_lib_base iscore_lib_state)

setup_iscore_library(iscore_lib_device)

