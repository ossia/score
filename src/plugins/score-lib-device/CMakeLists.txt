project(score_lib_device LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Xml)

# Files & main target
### Plugin ###
set(HEADERS
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/AddressSettings.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/ClipMode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/IOType.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/ItemModels/NodeBasedItemModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/ItemModels/NodeDisplayMethods.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Loading/ScoreDeviceLoader.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Loading/JamomaDeviceLoader.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Node/DeviceNode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Node/NodeListMimeSerialization.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/DeviceInterface.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/DeviceSettings.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/ProtocolFactoryInterface.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/ProtocolList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/ProtocolSettingsWidget.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Widgets/DeviceCompleter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Widgets/DeviceModelProvider.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Widgets/AddressAccessorEditWidget.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score_lib_device.hpp"
)

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/AddressSettingsSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/IOType.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/ClipMode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Address/AddressSettings.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Device/Node/DeviceNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Node/DeviceNodeSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/DeviceInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/DeviceSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/ProtocolFactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Protocol/ProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Widgets/DeviceModelProvider.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Device/ItemModels/NodeBasedItemModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/ItemModels/NodeDisplayMethods.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Device/Loading/JamomaDeviceLoader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Loading/ScoreDeviceLoader.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Device/Widgets/DeviceCompleter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Device/Widgets/AddressAccessorEditWidget.cpp"
)

add_library(score_lib_device ${SRCS} ${HEADERS})
target_link_libraries(score_lib_device
    PUBLIC Qt5::Core Qt5::Widgets score_lib_base score_lib_state
    PRIVATE Qt5::Xml)

setup_score_library(score_lib_device)

