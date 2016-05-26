cmake_minimum_required(VERSION 3.0)
project(iscore_lib_base LANGUAGES CXX)
iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Svg QuickWidgets)

# Files & main target
file(GLOB_RECURSE HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iscore/*.hpp)

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/resources/iscore.qrc")
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/serialization/StringConstants.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/menu/MenuInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/InvisibleRootNodeSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/ObjectPathSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/SettableIdentifierGeneration.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/CommandDataSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/customfactory/FactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/customfactory/FactoryFamily.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/customfactory/SerializableInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/ElementPluginModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/ElementPluginModelListSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/AggregateCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/Command.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/CommandData.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/CommandGeneratorMap.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/PropertyCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/command/SerializableCommand.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/component/Component.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/application/ApplicationContext.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/application/ApplicationComponents.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/actions/ActionManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/actions/Action.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/document/DocumentInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/locking/ObjectLocker.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/DocumentDelegateModelInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/DocumentDelegateViewInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/ElementPluginModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/documentdelegate/plugin/ElementPluginModelList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/application/GUIApplicationContextPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/panel/PanelDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/panel/PanelDelegateFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/settingsdelegate/SettingsDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/settingsdelegate/SettingsDelegateModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/settingsdelegate/SettingsDelegatePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/settingsdelegate/SettingsDelegateView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/ProjectSettings/ProjectSettingsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/ProjectSettings/ProjectSettingsModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/ProjectSettings/ProjectSettingsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/ProjectSettings/ProjectSettingsView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/plugins/qt_interfaces/CommandFactory_QtInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/selection/SelectionDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/selection/SelectionStack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/serialization/DataStreamVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/serialization/JSONObjectVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/serialization/JSONObjectSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/serialization/QtTypesJsonVisitors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/NamedObjectSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/ObjectIdentifierSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/IdentifiedObjectAbstract.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/NamedObject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/ObjectPath.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/RandomNameProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/statemachine/GraphicsSceneToolPalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/statemachine/CommonSelectionState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/widgets/GraphicsItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/widgets/ClearLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/widgets/DoubleSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/widgets/SetIcons.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/widgets/QmlContainerPanel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/core/application/ApplicationInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/ApplicationRegistrar.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/ApplicationSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStackSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/OpenDocumentsFile.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/CommandBackupFile.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackupManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackups.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBuilder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/Document.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/CoreApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/DocumentManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/MenubarManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/Widgets/UndoListWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPanelDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/UndoApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/view/View.cpp"

)

add_library(iscore_lib_base ${SRCS} ${HEADERS} ${QRCS})

target_link_libraries(iscore_lib_base
        PUBLIC Qt5::Core Qt5::Widgets Qt5::QuickWidgets
    PRIVATE Qt5::Svg QRecentFilesMenu)

set_property(TARGET iscore_lib_base PROPERTY COTIRE_CXX_PREFIX_HEADER_INIT "${CMAKE_CURRENT_SOURCE_DIR}/iscore/prefix.hpp")
setup_iscore_library(iscore_lib_base)
setup_iscore_tests(tests)
