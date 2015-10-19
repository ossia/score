cmake_minimum_required(VERSION 3.0)
project(iscore_lib_base)
iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Svg)

# Files & main target
file(GLOB_RECURSE HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iscore/*.hpp)

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/resources/iscore.qrc")
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/menu/MenuInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/InvisibleRootNodeSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/ObjectPathSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/SettableIdentifierSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/SettableIdentifierGeneration.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/ElementPluginModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/ElementPluginModelListSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/AggregateCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/Command.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/PropertyCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/SerializableCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/document/DocumentInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/locking/ObjectLocker.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/ElementPluginModelList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/plugincontrol/PluginControlInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/panel/PanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/panel/PanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/presenter/PresenterInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/selection/SelectionDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/selection/SelectionStack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/serialization/JSONObjectSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/serialization/QtTypesJsonVisitors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/NamedObjectSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/ObjectIdentifierSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/ObjectPath.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/statemachine/BaseStateMachine.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/statemachine/CommonSelectionState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/statemachine/ToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/widgets/GraphicsItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/widgets/ClearLayout.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/core/application/Application.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStackSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/OpenDocumentsFile.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/CommandBackupFile.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackupManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackups.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBuilder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/Document.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/MenubarManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/Widgets/UndoListWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/UndoControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/view/View.cpp"

)

add_library(iscore_lib_base ${SRCS} ${HEADERS} ${QRCS})
target_link_libraries(iscore_lib_base Qt5::Core Qt5::Widgets Qt5::Svg)

setup_iscore_library(iscore_lib_base)
setup_iscore_tests(tests)
