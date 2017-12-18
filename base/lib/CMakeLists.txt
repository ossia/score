cmake_minimum_required(VERSION 3.0)
project(score_lib_base LANGUAGES CXX)
score_common_setup()

# Packages
find_package(Qt5 5.7 REQUIRED COMPONENTS Core Widgets Svg QuickWidgets)

# Files & main target
file(GLOB_RECURSE HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/score/*.hpp)

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/resources/score.qrc")
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/StringConstants.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/AnySerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/InvisibleRootNodeSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/ObjectPathSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/IdentifierGeneration.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandDataSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/customfactory/FactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/customfactory/FactoryFamily.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/customfactory/SerializableInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/command/AggregateCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/Command.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandData.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandGeneratorMap.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/PropertyCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/Validity/ValidityChecker.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/model/Component.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationContext.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationComponents.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/actions/Action.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/actions/ActionManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/actions/MenuManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/actions/ToolbarManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/document/DocumentInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/locking/ObjectLocker.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegateModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegateView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegatePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/plugin/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/application/GUIApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/panel/PanelDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/panel/PanelDelegateFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegateModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegatePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegateView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/Addon.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/FactoryFamily_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/FactoryInterface_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/PluginRequirements_QtInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/CommandFactory_QtInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/selection/SelectionDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/selection/SelectionStack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/DataStreamVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/JSONObjectVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/QtTypesJsonVisitors.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/ObjectIdentifierSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/IdentifiedObjectAbstract.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/ObjectPath.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/RelativePath.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RandomNameProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/ModelMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/Skin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/ColorReference.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/GraphicsSceneToolPalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/CommonSelectionState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/GraphicsItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ClearLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SetIcons.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/QmlContainerPanel.cpp" # TODO MOVEME
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Separator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/GraphicWidgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/JS/JSEdit.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/core/application/SafeQApplication.cpp"
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

add_library(score_lib_base ${SRCS} ${HEADERS} ${QRCS})

target_link_libraries(score_lib_base
        PUBLIC Qt5::Core Qt5::Widgets Qt5::Qml Qt5::QuickWidgets ossia
    PRIVATE Qt5::Svg QRecentFilesMenu)

set_property(TARGET score_lib_base PROPERTY COTIRE_CXX_PREFIX_HEADER_INIT "${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp")
setup_score_library(score_lib_base)
setup_score_tests(tests)
