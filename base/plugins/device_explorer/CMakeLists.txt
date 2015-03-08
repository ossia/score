cmake_minimum_required(VERSION 3.0)
project(device_explorer_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

# Files & main target

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

file(GLOB_RECURSE INTERFACE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/DeviceInterface/*.cpp")
file(GLOB_RECURSE INTERFACE_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/DeviceInterface/*.hpp")
set(INTERFACE_SRCS ${INTERFACE_SRCS}
${CMAKE_CURRENT_SOURCE_DIR}/QMenuView/qmenuview.cpp
${CMAKE_CURRENT_SOURCE_DIR}/QMenuView/qmenuview.h
${CMAKE_CURRENT_SOURCE_DIR}/QMenuView/qmenuview_p.h
)


set(COMMANDS_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerPasteCommand.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerRemoveCommand.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerMoveCommand.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerInsertCommand.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerCutCommand.cpp
#${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerCopyCommand.cpp
)
set(COMMANDS_HDRS
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerPasteCommand.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerRemoveCommand.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerMoveCommand.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerInsertCommand.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerCutCommand.hpp
#${CMAKE_CURRENT_SOURCE_DIR}/Commands/DeviceExplorerCopyCommand.hpp
)

set(PANEL_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/Panel/IOTypeDelegate.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceEditDialog.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressEditDialog.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerModel.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerFilterProxyModel.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerWidget.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressStringSettingsWidget.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/OSCProtocolSettingsWidget.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/MIDIProtocolSettingsWidget.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/MinuitProtocolSettingsWidget.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressFloatSettingsWidget.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressIntSettingsWidget.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerView.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/Node.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/NodeFactory.cpp

)

set(PANEL_HDRS
${CMAKE_CURRENT_SOURCE_DIR}/Panel/IOTypeDelegate.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceEditDialog.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressEditDialog.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerModel.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerFilterProxyModel.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressStringSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/OSCProtocolSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/MIDIProtocolSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/ProtocolSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/MinuitProtocolSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressFloatSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/AddressIntSettingsWidget.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/DeviceExplorerView.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/Node.hpp
${CMAKE_CURRENT_SOURCE_DIR}/Panel/NodeFactory.hpp
)

set(PLUGIN_FILES
${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorerPanelFactory.hpp
${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorerPanelFactory.cpp
${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorerPlugin.hpp
${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorerPlugin.cpp
)

file(GLOB_RECURSE QRC "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc")

qt5_add_resources(EXPLORER_RESOURCES_RCC "${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer.qrc")

add_library(device_explorer_plugin
						${INTERFACE_SRCS} ${INTERFACE_HDRS}
			${COMMANDS_HDRS} ${COMMANDS_SRCS}
			${PANEL_HDRS} ${PANEL_SRCS}
			${PLUGIN_FILES} ${EXPLORER_RESOURCES_RCC})



target_link_libraries(device_explorer_plugin Qt5::Core Qt5::Widgets iscore_lib)


install(TARGETS device_explorer_plugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
