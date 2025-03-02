if(APPLE)
  project(score_lib_base LANGUAGES CXX OBJC OBJCXX)
else()
  project(score_lib_base LANGUAGES CXX)
endif()

score_common_setup()

# Packages
find_package(${QT_VERSION} QUIET OPTIONAL_COMPONENTS Svg)

file(GENERATE
  OUTPUT
    zipdownloader_export.h
  CONTENT
    "#pragma once\n#include <score_lib_base_export.h>\n#define ZIPDOWNLOADER_EXPORT SCORE_LIB_BASE_EXPORT\n"
)

file(GENERATE
  OUTPUT
    rnd_export.h
  CONTENT
    "#pragma once\n#include <score_lib_base_export.h>\n#define RND_EXPORT SCORE_LIB_BASE_EXPORT\n"
)

if(SCORE_INSTALL_HEADERS)
  install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/zipdownloader_export.h
        ${CMAKE_CURRENT_BINARY_DIR}/rnd_export.h
        DESTINATION include/score
        COMPONENT Devel
        OPTIONAL)
endif()

# Files & main target
include(ScoreQtConf)

qt_add_resources(QRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/score.qrc"
  "${CMAKE_CURRENT_BINARY_DIR}/qtconf.qrc"
)

set_source_files_properties(
  ${QRCS}
  PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION 1
)

set(HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/ApplicationInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/ApplicationRegistrar.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/ApplicationSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/CommandBackupFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/MockApplication.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/OpenDocumentsFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/SafeQApplication.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/MinimalApplication.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStack.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStackSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/Document.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackupManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackups.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBuilder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentMetadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/documentation/intro.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/documentation/models.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/messages/MessagesPanel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginDependencyGraph.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/AboutDialog.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/CoreActions.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/CoreApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/DocumentManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPanelFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/Widgets/UndoListWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/undo/UndoApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/view/Window.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/view/FixedTabWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/view/HelperPanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/view/QRecentFilesMenu.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/score/actions/Action.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/actions/ActionManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/actions/Menu.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/actions/MenuManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/actions/Toolbar.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/actions/ToolbarManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationComponents.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationContext.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationServices.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/application/GUIApplicationContext.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/AggregateCommand.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Command.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandFactoryKey.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandGeneratorMap.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandStackFacade.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/CommandDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/ICommandDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/MacroCommandDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/OngoingCommandDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/RuntimeDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/SendStrategy.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/SettingsCommandDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/PropertyCommand.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/SettingsCommand.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/command/Validity/ValidityChecker.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/document/ChangeId.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/document/DocumentContext.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/document/DocumentInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/gfx/OpenGL.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/gfx/Vulkan.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/locking/ObjectLocker.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ColorInterpolator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ColorReference.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/Component.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ComponentFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ComponentHierarchy.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ComponentSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ComponentUtils.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/Entity.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/EntityBase.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/EntityMap.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/EntityList.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/EntityImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/EntitySerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/EntityMapSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/HierarchicalModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/IdentifiedObject.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/IdentifiedObjectAbstract.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/IdentifiedObjectMap.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/Identifier.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/IdentifierDebug.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ModelMetadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/Skin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/ObjectIdentifier.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/ObjectPath.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/ObjectEditor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/Path.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/PathDebug.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/PathSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/InvisibleRootNode.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/TreeNode.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/TreeNodeItemModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/TreeNodeSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/TreePath.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/VariantBasedNode.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/Addon.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/DeserializeKnownSubType.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/PluginInstances.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/application/GUIApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/InterfaceList.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/Interface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/FactorySetup.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ModelFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/SerializableInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/SerializableHelpers.hpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/DeserializeKnownSubtype.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/StringFactoryKey.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/StringFactoryKeySerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/UuidKey.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegateFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegateModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegatePresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/DocumentDelegateView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/plugin/DocumentPluginBase.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/plugin/SerializableDocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/plugin/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/panel/PanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/panel/PanelDelegateFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegateFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegateModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/settingsdelegate/SettingsDelegateView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/selection/FocusManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/selection/Selectable.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/selection/Selection.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/selection/SelectionDispatcher.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/selection/SelectionStack.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/AnySerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/DataStreamVisitor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/IsTemplate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/CommonTypes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/JSONValueVisitor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/JSONVisitor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/MapSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/MimeVisitor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/StdVariantSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/StringConstants.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/VariantSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/VisitorCommon.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/VisitorInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/VisitorTags.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/CommonSelectionState.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/GraphicsSceneTool.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/GraphicsSceneToolPalette.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/StateMachineTools.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/StateMachineUtils.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Clamp.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cursor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/DeleteAll.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/File.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/FindStringInFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/IdentifierGeneration.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/MapCopy.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Metadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/QMapHelper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RandomNameProvider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/SubtypeVariant.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ObjectMatches.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/FileWatch.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ForEach.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Bind.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/SafeCast.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ListNetworkAddresses.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Unused.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RecursiveWatch.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ThreadPool.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Debug.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Version.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/exceptions/MissingCommand.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/opaque/OpaqueString.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/Algorithms.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/ArrayView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/HashMap.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/HashMapSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/IndirectContainer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/Invoke.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/Optional.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/OwningVector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/Pointer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/String.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/StringHash.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ArrowDialog.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicWidgets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsSliderBaseImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/DefaultControlImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/DefaultGraphicsSliderImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/DefaultGraphicsKnobImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsProxyObject.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/InfiniteScroller.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ItemBounder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/PainterPath.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/RectItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/TextItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ZoomItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/YPos.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsBoxLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsGridLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsSplitLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsTabLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/Constants.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsButton.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsTextButton.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsCheckBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsCombo.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsEnum.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsHSVChooser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsIntSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsKnob.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLineEdit.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLogKnob.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLogSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsMultiSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsNoteChooser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPixmapButton.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPixmapEnum.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPixmapToggle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsSelectablePixmapToggle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsRangeSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsSliderBase.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsTimeChooser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsToggle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYChooser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYZChooser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYSpinbox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYZSpinbox.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/AddRemoveList.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ArrowButton.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ClearLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ComboBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/CommandSpinBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ControlWidgets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSpinBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IconProvider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IntSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/FormWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Layout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MarginLess.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MessageBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MimeData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Pixmap.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/PluginWindow.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/QuantificationWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ReactiveLabel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Separator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SelectionButton.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SetIcons.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SignalUtils.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/StyleSheets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SpinBoxes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SearchLineEdit.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/TimeMode.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/TimeSpinBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/TextLabel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/WidgetWrapper.hpp"

     "${3RDPARTY_FOLDER}/QProgressIndicator/QProgressIndicator.h"

     "${3RDPARTY_FOLDER}/QMenuView/qmenuview.h"
     "${3RDPARTY_FOLDER}/QMenuView/qmenuview_p.h"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_palette_model.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_palette_widget.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/colorwidgets_global.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_wheel.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_line_edit.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/swatch.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_delegate.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_palette.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/hue_slider.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/abstract_widget_list.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/bound_color_selector.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_dialog.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_2d_slider.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_selector.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_list_widget.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_preview.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/gradient_slider.hpp"
     #"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Qt-Color-Widgets/QtColorWidgets/color_names.hpp"
)

set(SRCS
"${3RDPARTY_FOLDER}/QMenuView/qmenuview.cpp"
"${3RDPARTY_FOLDER}/zipdownloader/src/miniz.cpp"
"${3RDPARTY_FOLDER}/zipdownloader/src/zipdownloader.cpp"
"${OSSIA_3RDPARTY_FOLDER}/rnd/src/random.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/StringConstants.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/AnySerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/tree/InvisibleRootNodeSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/path/ObjectPathSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/IdentifierGeneration.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandDataSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/gfx/OpenGL.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/gfx/Vulkan.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/FactorySetup.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/Interface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/InterfaceList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/SerializableInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/command/AggregateCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/Command.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandData.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/CommandGeneratorMap.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/PropertyCommand.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/Validity/ValidityChecker.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/command/Dispatchers/SendStrategy.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/model/Component.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/ColorInterpolator.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationContext.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationComponents.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/application/ApplicationServices.cpp"

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

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/ProjectSettings/ProjectSettingsModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/Addon.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/PluginInstances.cpp"

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
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/ObjectEditor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/ModelMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/Skin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/model/ColorReference.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/GraphicsSceneToolPalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/statemachine/CommonSelectionState.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/std/String.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/File.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/FileWatch.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RandomNameProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RecursiveWatch.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ThreadPool.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ArrowDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicWidgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/RectItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/TextItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ZoomItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsBoxLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsGridLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsSplitLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsTabLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/Constants.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsTextButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsCheckBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsCombo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsEnum.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsHSVChooser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsIntSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsKnob.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLineEdit.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLogKnob.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLogSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsMultiSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsNoteChooser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPixmapButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPixmapEnum.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPixmapToggle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsRangeSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsSelectablePixmapToggle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsSpinbox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsTimeChooser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsToggle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYChooser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYZChooser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYSpinbox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsXYZSpinbox.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/AddRemoveList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ArrowButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ClearLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ControlWidgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSpinBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/FormWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IconProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IntSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MessageBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/PluginWindow.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/QuantificationWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SetIcons.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SelectionButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Separator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SpinBoxes.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ListNetworkAddresses.cpp"

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
"${CMAKE_CURRENT_SOURCE_DIR}/core/messages/MessagesPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/AboutDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/CoreApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/DocumentManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/Widgets/UndoListWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPanelDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/Panel/UndoPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/undo/UndoApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/view/Window.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/view/FixedTabWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/view/QRecentFilesMenu.cpp"

"${3RDPARTY_FOLDER}/QProgressIndicator/QProgressIndicator.cpp"

# "${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_list_widget.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/gradient_slider.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_line_edit.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/swatch.cpp"
# "${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/bound_color_selector.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_palette_widget.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_palette_model.cpp"
# "${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_2d_slider.cpp"
# "${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/abstract_widget_list.cpp"
# "${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_delegate.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_utils.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_dialog.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_palette.cpp"
# "${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_selector.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_preview.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/hue_slider.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_wheel.cpp"
"${3RDPARTY_FOLDER}/Qt-Color-Widgets/src/color_names.cpp"
)

add_library(score_lib_base ${SRCS} ${HEADERS} ${QRCS})

get_property(CMAKE_GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set_source_files_properties( "${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp"
    PROPERTIES
     COMPILE_DEFINITIONS
      "CMAKE_GENERATOR_IS_MULTI_CONFIG=$<BOOL:${CMAKE_GENERATOR_IS_MULTI_CONFIG}>;CMAKE_GENERATOR_CONFIGURATION=\"$<CONFIG>\""
)

if(APPLE)
    find_library(AppKit_FK AppKit)
    find_library(AppServices_FK ApplicationServices)
    target_sources(score_lib_base PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cursor.mm"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${AppKit_FK}
        ${AppServices_FK}
    )
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cursor.mm"
        PROPERTIES
          SKIP_UNITY_BUILD_INCLUSION 1
    )
endif()

target_include_directories(score_lib_base SYSTEM PUBLIC "${3RDPARTY_FOLDER}")
target_include_directories(score_lib_base SYSTEM PUBLIC "${3RDPARTY_FOLDER}/zipdownloader/src")
target_include_directories(score_lib_base SYSTEM PUBLIC "${OSSIA_3RDPARTY_FOLDER}/rnd/include")
target_include_directories(score_lib_base SYSTEM PRIVATE "${3RDPARTY_FOLDER}/quickcpplib/include")
target_include_directories(score_lib_base SYSTEM PRIVATE "${3RDPARTY_FOLDER}/outcome/include")
target_include_directories(score_lib_base SYSTEM PRIVATE "${3RDPARTY_FOLDER}/llfio/include")

target_link_libraries(score_lib_base
  PUBLIC
    ${QT_PREFIX}::Core
    ${QT_PREFIX}::Gui ${QT_PREFIX}::GuiPrivate
    ${QT_PREFIX}::Widgets ${QT_PREFIX}::WidgetsPrivate
    ${QT_PREFIX}::Network
    ossia
    QCodeEditor
    magic_enum::magic_enum
)

if(TARGET ${QT_PREFIX}::Svg)
  target_link_libraries(score_lib_base
    PRIVATE ${QT_PREFIX}::Svg
  )
endif()
if(TARGET ${QT_PREFIX}::StateMachine)
  target_link_libraries(score_lib_base
    PRIVATE ${QT_PREFIX}::StateMachine
  )
endif()
target_include_directories(score_lib_base PUBLIC ${3RDPARTY_FOLDER}/QProgressIndicator)
target_include_directories(score_lib_base PUBLIC ${3RDPARTY_FOLDER}/Qt-Color-Widgets)
target_include_directories(score_lib_base PUBLIC ${3RDPARTY_FOLDER}/Qt-Color-Widgets/src)
target_include_directories(score_lib_base PUBLIC ${3RDPARTY_FOLDER}/Qt-Color-Widgets/QtColorWidgets)
target_include_directories(score_lib_base PUBLIC ${OSSIA_3RDPARTY_FOLDER}/verdigris/src)

setup_score_library(score_lib_base)

if(SCORE_PCH)
  if(APPLE)
    target_precompile_headers(score_lib_base PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp>"
      "$<$<COMPILE_LANGUAGE:OBJC>:${CMAKE_CURRENT_SOURCE_DIR}/score/dummy.h>"
      "$<$<COMPILE_LANGUAGE:OBJCXX>:${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp>"
    )
  else()
    target_precompile_headers(score_lib_base PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp>"
    )
  endif()
endif()

if(SCORE_PCH)
  # Create a dummy library which will be used to propagate the PCH to all the plug-ins
  # We cannot use score_lib_base's directly as it mau have SCORE_LIB_BASE_EXPORT defined
  # to dllexport - building it separately ensures that it will be dllimport instead.
  add_library(score_lib_pch STATIC core/dummy.cpp)
  target_link_libraries(score_lib_pch PUBLIC score_lib_base)
  setup_score_library(score_lib_pch)
  target_compile_definitions(score_lib_pch PUBLIC SCORE_LIB_PCH_EXPORTS)

  if(APPLE)
    target_sources(score_lib_pch PRIVATE core/dummy.mm core/dummy.m)
    target_precompile_headers(score_lib_pch PUBLIC
      "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp>"
      "$<$<COMPILE_LANGUAGE:OBJC>:${CMAKE_CURRENT_SOURCE_DIR}/core/dummy.h>"
      "$<$<COMPILE_LANGUAGE:OBJCXX>:${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp>"
    )
  else()
    target_precompile_headers(score_lib_pch PUBLIC
      "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/score/prefix.hpp>"
    )
  endif()
endif()

set(_score_license_written 0 CACHE INTERNAL "")
if(NOT _score_license_written)
  set(_score_license_written 1 CACHE INTERNAL "")
  include(GenerateLicense)
  set(license_text "")
  AddLicenseFile(license_text ossia "${CMAKE_SOURCE_DIR}/3rdparty/libossia/LICENSE")
  AddLicenseFile(license_text score "${CMAKE_SOURCE_DIR}/LICENSE.txt")
  AddLicenseFile(license_text Aether "${CMAKE_SOURCE_DIR}/3rdparty/Aether/LICENSE.md")
  AddLicenseFile(license_text Catch2 "${OSSIA_3RDPARTY_FOLDER}/Catch2/LICENSE.txt")
  AddLicenseFile(license_text concurrentqueue "${OSSIA_3RDPARTY_FOLDER}/concurrentqueue/LICENSE.md")
  AddLicenseFile(license_text CTRE "${OSSIA_3RDPARTY_FOLDER}/compile-time-regular-expressions/LICENSE")
  AddLicenseFile(license_text dno "${OSSIA_3RDPARTY_FOLDER}/dno/LICENSE")
  AddLicenseFile(license_text dnssd "${OSSIA_3RDPARTY_FOLDER}/Servus/3rdparty/dnssd/LICENSE")
  AddLicenseFile(license_text dr_libs "${OSSIA_3RDPARTY_FOLDER}/dr_libs/LICENSE")
  # AddLicenseFile(license_text DSPFilters "${CMAKE_SOURCE_DIR}/3rdparty/DSPFilters/LICENSE") # defined in .cpp
  AddLicenseFile(license_text Flicks "${OSSIA_3RDPARTY_FOLDER}/Flicks/LICENSE")
  AddLicenseFile(license_text fmt "${OSSIA_3RDPARTY_FOLDER}/fmt/LICENSE.rst")
  AddLicenseFile(license_text Gamma "${CMAKE_SOURCE_DIR}/3rdparty/Gamma/LICENSE")
  AddLicenseFile(license_text Guitarix "${CMAKE_SOURCE_DIR}/3rdparty/Guitarix/COPYING")
  AddLicenseFile(license_text Gist "${CMAKE_SOURCE_DIR}/3rdparty/Gist/LICENSE.txt")
  AddLicenseFile(license_text hap "${CMAKE_SOURCE_DIR}/3rdparty/hap/LICENSE")
  AddLicenseFile(license_text hsluv "${CMAKE_SOURCE_DIR}/3rdparty/phantomstyle/LICENSE-hsluv-c.md")
  AddLicenseFile(license_text kfr "${OSSIA_3RDPARTY_FOLDER}/kfr/LICENSE.txt")
  AddLicenseFile(license_text libartnet "${OSSIA_3RDPARTY_FOLDER}/libartnet/COPYING")
  AddLicenseFile(license_text libe131 "${OSSIA_3RDPARTY_FOLDER}/libe131/LICENSE")
  AddLicenseFile(license_text libremidi "${OSSIA_3RDPARTY_FOLDER}/libremidi/LICENSE.RtMidi")
  AddLicenseFile(license_text libsamplerate "${OSSIA_3RDPARTY_FOLDER}/libsamplerate/COPYING")
  AddLicenseFile(license_text libsndfile "${CMAKE_SOURCE_DIR}/3rdparty/libsndfile/COPYING")
  AddLicenseFile(license_text mdspan "${OSSIA_3RDPARTY_FOLDER}/mdspan/LICENSE")
  AddLicenseFile(license_text miniz "${CMAKE_SOURCE_DIR}/3rdparty/zipdownloader/LICENSE.miniz")
  AddLicenseFile(license_text modernmidi "${OSSIA_3RDPARTY_FOLDER}/libremidi/LICENSE.ModernMidi")
  AddLicenseFile(license_text nanosignal "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot/LICENSE")
  AddLicenseFile(license_text oscpack "${OSSIA_3RDPARTY_FOLDER}/oscpack/LICENSE")
  AddLicenseFile(license_text pd "${CMAKE_SOURCE_DIR}/3rdparty/libpd/pure-data/LICENSE.txt")
  AddLicenseFile(license_text libpd "${CMAKE_SOURCE_DIR}/3rdparty/libpd/LICENSE.txt")
  AddLicenseFile(license_text perlinnoise "${OSSIA_3RDPARTY_FOLDER}/PerlinNoise/LICENSE")
  AddLicenseFile(license_text phantomstyle "${CMAKE_SOURCE_DIR}/3rdparty/phantomstyle/LICENSE")
  AddLicenseFile(license_text portaudio "${CMAKE_SOURCE_DIR}/3rdparty/libpd/pure-data/portaudio/portaudio/LICENSE.txt")
  AddLicenseFile(license_text pybind11 "${OSSIA_3RDPARTY_FOLDER}/pybind11/LICENSE")
  AddLicenseFile(license_text QCodeEditor "${CMAKE_SOURCE_DIR}/3rdparty/QCodeEditor/LICENSE.MIT")
  AddLicenseFile(license_text qprogressindicator "${SCORE_SRC}/lib/3rdparty/QProgressIndicator/LICENSE")
  AddLicenseFile(license_text qtcolorwidgets "${SCORE_SRC}/lib/3rdparty/Qt-Color-Widgets/COPYING")
  AddLicenseFile(license_text rapidjson "${OSSIA_3RDPARTY_FOLDER}/rapidjson/license.txt")
  AddLicenseFile(license_text readerwriterqueue "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue/LICENSE.md")
  AddLicenseFile(license_text re2 "${OSSIA_3RDPARTY_FOLDER}/re2/LICENSE")
  AddLicenseFile(license_text rnd "${OSSIA_3RDPARTY_FOLDER}/rnd/LICENSE")
  AddLicenseFile(license_text rubberband "${OSSIA_3RDPARTY_FOLDER}/rubberband/COPYING")
  AddLicenseFile(license_text servus "${OSSIA_3RDPARTY_FOLDER}/Servus/LICENSE.txt")
  AddLicenseFile(license_text shmdata "${CMAKE_SOURCE_DIR}/3rdparty/shmdata/LICENSE.md")
  AddLicenseFile(license_text sh4lt "${CMAKE_SOURCE_DIR}/3rdparty/sh4lt/LICENSE.md")
  AddLicenseFile(license_text span "${OSSIA_3RDPARTY_FOLDER}/span/LICENSE_1_0.txt")
  AddLicenseFile(license_text spout "${CMAKE_SOURCE_DIR}/3rdparty/Spout/LICENSE")
  AddLicenseFile(license_text syphon "${CMAKE_SOURCE_DIR}/3rdparty/Syphon-Framework/License.txt")
  AddLicenseFile(license_text smallfun "${OSSIA_3RDPARTY_FOLDER}/SmallFunction/LICENSE")
  AddLicenseFile(license_text snappy "${CMAKE_SOURCE_DIR}/3rdparty/snappy/COPYING")
  AddLicenseFile(license_text spdlog "${OSSIA_3RDPARTY_FOLDER}/spdlog/LICENSE")
  AddLicenseFile(license_text tuplet "${OSSIA_3RDPARTY_FOLDER}/tuplet/LICENSE")
  AddLicenseFile(license_text verdigris "${OSSIA_3RDPARTY_FOLDER}/verdigris/LICENSE.LGPLv3")
  AddLicenseFile(license_text vst3 "${CMAKE_SOURCE_DIR}/3rdparty/vst3/LICENSE.txt")
  AddLicenseFile(license_text whereami "${OSSIA_3RDPARTY_FOLDER}/whereami/LICENSE.MIT")
  AddLicenseFile(license_text wiiuse "${OSSIA_3RDPARTY_FOLDER}/wiiuse/LICENSE")
  AddLicenseFile(license_text websocketpp "${OSSIA_3RDPARTY_FOLDER}/websocketpp/COPYING")
  AddLicenseFile(license_text zipdownloader "${CMAKE_SOURCE_DIR}/3rdparty/zipdownloader/LICENSE")


  score_write_file("${CMAKE_BINARY_DIR}/score_licenses.hpp" "${license_text}")
endif()
