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

if(APPLE)
  set(SCORE_FONT_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts_macos.qrc")
else()
  set(SCORE_FONT_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts.qrc")
endif()
qt_add_resources(QRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/score.qrc"
  "${CMAKE_CURRENT_BINARY_DIR}/qtconf.qrc"
  "${SCORE_FONT_RESOURCE}"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/WasmLogging.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/application/MinimalApplication.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStack.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStackSerialization.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/Document.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackupManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBackups.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentBuilder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentTemplates.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/OnlineExamples.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/document/ProjectInfo.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Licenses.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/AboutWidget.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/core/view/CentralViewStack.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/view/WidgetArgTypes.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/application/NetworkSessionInterface.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/score/serialization/JSONParse.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cuda.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cursor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/DeleteAll.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/File.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/FindStringInFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/IdentifierGeneration.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/MapCopy.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Metadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLock.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/QMapHelper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RandomNameProvider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/SubtypeVariant.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ObjectMatches.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/MDMEnrollmentDetection.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/FileWatch.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ProjectFiles.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Zip.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ForEach.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Bind.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/SafeCast.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ListNetworkAddresses.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Platforms.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Unused.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RecursiveWatch.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ThreadPool.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Timers.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/BackgroundRenderer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/BangPainting.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicWidgets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsSliderBaseImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/DefaultControlImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/DefaultGraphicsSliderImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/DefaultGraphicsKnobImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/WidgetPresentation.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsProxyObject.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/InfiniteScroller.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ItemBounder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/PainterPath.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/RightClickWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/RectItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/TextItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/TypeInWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ZoomItem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/YPos.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsBoxLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsGridLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsSplitLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsTabLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsStripDetailLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/LayoutSelection.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/Constants.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsButton.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsTextButton.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsCheckBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsCombo.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/Stepper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsEnum.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsHSVChooser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsIntSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsKnob.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLineEdit.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLogKnob.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsLogSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsMultiSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsMultiSliderXY.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPathGeneratorXY.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/BackgroundRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ClearLayout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ComboBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/CommandSpinBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ControlWidgets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ApplicationStyle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ValidationPalette.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSpinBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IconProvider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IntSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/FormWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ItemViewDrag.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Layout.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MarginLess.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MessageBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MimeData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Pixmap.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/PluginWindow.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/PromptLineEdit.hpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/score/plugins/application/NetworkSessionInterface.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cuda.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/File.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/FileContains.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/FileWatch.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ProjectFiles.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Zip.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/MDMEnrollmentDetection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLock.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RandomNameProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/RecursiveWatch.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ThreadPool.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ArrowDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/WasmInputMethod.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicsLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/WidgetPresentation.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/GraphicWidgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/InfiniteScroller.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/RectItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/TextItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/TypeInWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/ZoomItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsBoxLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsGridLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsSplitLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsTabLayout.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/layouts/GraphicsStripDetailLayout.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsMultiSliderXY.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/graphics/widgets/QGraphicsPathGeneratorXY.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ComboBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ControlWidgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ApplicationStyle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ValidationPalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/DoubleSpinBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/FormWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IconProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/IntSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/ItemViewDrag.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/MessageBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/PluginWindow.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/PromptLineEdit.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/QuantificationWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SetIcons.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SelectionButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/Separator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/widgets/SpinBoxes.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/ListNetworkAddresses.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Platforms.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Timers.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/core/application/SafeQApplication.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/WasmLogging.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentTemplates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/OnlineExamples.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/ProjectInfo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/Document.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/messages/MessagesPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/AboutDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Licenses.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/AboutWidget.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/core/view/CentralViewStack.cpp"
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

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLock.cpp"
    PROPERTIES
      SKIP_UNITY_BUILD_INCLUSION 1
)

if(APPLE)
    find_library(AppKit_FK AppKit)
    find_library(AppServices_FK ApplicationServices)
    target_sources(score_lib_base PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cursor.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLock.mm"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${AppKit_FK}
        ${AppServices_FK}
    )
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/Cursor.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLock.mm"
        PROPERTIES
          SKIP_UNITY_BUILD_INCLUSION 1
    )
endif()

# Relative pointer motion. On Wayland Qt exposes neither zwp_pointer_constraints_v1
# nor zwp_relative_pointer_manager_v1, and QCursor::setPos does nothing there; on X11
# plain MotionNotify stops at the screen edges, so XInput2 raw events are needed.
if(UNIX AND NOT APPLE AND NOT WIN32 AND NOT EMSCRIPTEN)
  find_package(PkgConfig QUIET)
  find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
  if(PkgConfig_FOUND AND WAYLAND_SCANNER_EXECUTABLE)
    pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
    pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
  endif()

  if(PkgConfig_FOUND)
    pkg_check_modules(SCORE_XINPUT2 QUIET IMPORTED_TARGET xcb xcb-xinput)
  endif()

  if(SCORE_XINPUT2_FOUND)
    target_sources(score_lib_base PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLockX11.hpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLockX11.cpp")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLockX11.cpp"
      PROPERTIES
        SKIP_UNITY_BUILD_INCLUSION 1)
    target_link_libraries(score_lib_base PRIVATE PkgConfig::SCORE_XINPUT2)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLock.cpp"
      APPEND PROPERTY
        COMPILE_DEFINITIONS SCORE_HAS_X11_POINTER_LOCK=1)
  endif()

  if(WAYLAND_CLIENT_FOUND AND WAYLAND_PROTOCOLS_FOUND)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

    set(SCORE_WAYLAND_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols")
    file(MAKE_DIRECTORY "${SCORE_WAYLAND_GENERATED_DIR}")

    foreach(proto
        "relative-pointer/relative-pointer-unstable-v1"
        "pointer-constraints/pointer-constraints-unstable-v1")
      get_filename_component(proto_name "${proto}" NAME)
      set(proto_xml "${WAYLAND_PROTOCOLS_DIR}/unstable/${proto}.xml")
      set(proto_header "${SCORE_WAYLAND_GENERATED_DIR}/${proto_name}-client-protocol.h")
      set(proto_source "${SCORE_WAYLAND_GENERATED_DIR}/${proto_name}-protocol.c")

      add_custom_command(
        OUTPUT "${proto_header}"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${proto_xml}" "${proto_header}"
        DEPENDS "${proto_xml}"
        VERBATIM)
      add_custom_command(
        OUTPUT "${proto_source}"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${proto_xml}" "${proto_source}"
        DEPENDS "${proto_xml}"
        VERBATIM)

      set_source_files_properties("${proto_source}"
        PROPERTIES
          SKIP_UNITY_BUILD_INCLUSION 1
          SKIP_PRECOMPILE_HEADERS 1)

      target_sources(score_lib_base PRIVATE "${proto_header}" "${proto_source}")
    endforeach()

    target_include_directories(score_lib_base PRIVATE "${SCORE_WAYLAND_GENERATED_DIR}")
    target_link_libraries(score_lib_base PRIVATE PkgConfig::WAYLAND_CLIENT)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/score/tools/PointerLock.cpp"
      APPEND PROPERTY
        COMPILE_DEFINITIONS SCORE_HAS_WAYLAND_POINTER_LOCK=1)
  endif()
endif()

target_include_directories(score_lib_base SYSTEM PUBLIC "${3RDPARTY_FOLDER}")
target_include_directories(score_lib_base SYSTEM PUBLIC "${3RDPARTY_FOLDER}/zipdownloader/src")
target_include_directories(score_lib_base SYSTEM PUBLIC "${OSSIA_3RDPARTY_FOLDER}/rnd/include")

target_link_libraries(score_lib_base
  PUBLIC
    ${QT_PREFIX}::Core
    ${QT_PREFIX}::Gui ${QT_PREFIX}::GuiPrivate
    ${QT_PREFIX}::Widgets ${QT_PREFIX}::WidgetsPrivate
    ${QT_PREFIX}::Network
    ossia
    QCodeEditor
    magic_enum::magic_enum
    Eigen3::Eigen
    xtensor
  PRIVATE
    outcome
    llfio
    quickcpplib
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

# Components of score_lib_base and libossia. Plug-ins register their own
# dependencies; the header is written once all of src/ is configured.
set(_t "${3RDPARTY_FOLDER}")
set(_l "${OSSIA_3RDPARTY_FOLDER}")
score_register_license(score URL "https://ossia.io" FILES "${CMAKE_SOURCE_DIR}/LICENSE.txt")
score_register_license(libossia URL "https://ossia.io" FILES "${CMAKE_SOURCE_DIR}/3rdparty/libossia/LICENSE")
set(_qt_attributions "")
score_qt_sbom_attributions("${QT6_INSTALL_PREFIX}/${QT6_INSTALL_ARCHDATA}/sbom"
  "${CMAKE_CURRENT_BINARY_DIR}/qt-third-party.txt" _qt_sbom)
if(_qt_sbom)
  set(_qt_attributions "${CMAKE_CURRENT_BINARY_DIR}/qt-third-party.txt")
endif()
score_register_license(Qt URL "https://www.qt.io" HEADER "GNU General Public License v3"
  FILES ${_qt_attributions})
# A system Boost installs no license file.
file(GLOB _boost_license "${_l}/boost_1_*/LICENSE_1_0.txt")
if(_boost_license)
  list(GET _boost_license 0 _boost_license)
  score_register_license(Boost URL "https://www.boost.org" FILES "${_boost_license}")
else()
  score_register_license(Boost URL "https://www.boost.org" HEADER "Boost Software License 1.0")
endif()
score_register_license(CMake URL "https://www.cmake.org" HEADER "BSD 3-clause License")

score_register_license(abseil URL "https://github.com/abseil/abseil-cpp" FILES "${_l}/abseil-cpp/LICENSE")
score_register_license(Aether URL "https://github.com/Dougal-S/Aether" FILES "${_t}/Aether/LICENSE.md")
score_register_license(Catch2 URL "https://github.com/catchorg/Catch2" FILES "${_l}/Catch2/LICENSE.txt")
score_register_license(ConcurrentQueue URL "https://github.com/cameron314/concurrentqueue" FILES "${_l}/concurrentqueue/LICENSE.md")
score_register_license(CTRE URL "https://github.com/hanickadot/compile-time-regular-expressions" FILES "${_l}/compile-time-regular-expressions/LICENSE")
score_register_license(dno URL "https://github.com/thibaudk/dno" FILES "${_l}/dno/LICENSE")
score_register_license(dr_libs URL "https://github.com/mackron/dr_libs" FILES "${_l}/dr_libs/LICENSE")
score_register_license(DSPFilters URL "https://github.com/vinniefalco/DSPFilters" NOTICES "${_t}/DSPFilters/DSPFilters/include/DspFilters/Common.h")
score_register_license(Eigen URL "https://eigen.tuxfamily.org"
  FILES "${_t}/eigen/COPYING.README" "${_t}/eigen/COPYING.MPL2" "${_t}/eigen/COPYING.BSD"
        "${_t}/eigen/COPYING.APACHE" "${_t}/eigen/COPYING.MINPACK")
score_register_license(exprtk URL "https://www.partow.net/programming/exprtk" FILES "${_l}/exprtk/license.txt")
score_register_license(Flicks URL "https://github.com/OculusVR/Flicks" FILES "${_l}/Flicks/LICENSE")
score_register_license(fmt URL "https://github.com/fmtlib/fmt" FILES "${_l}/fmt/LICENSE")
score_register_license(Gamma URL "https://github.com/LancePutnam/Gamma" FILES "${_t}/Gamma/LICENSE")
score_register_license("HAP Codec" URL "https://github.com/Vidvox/hap" FILES "${_t}/hap/LICENSE")
score_register_license(hsluvc URL "https://github.com/hsluv/hsluv-c" FILES "${_t}/phantomstyle/LICENSE-hsluv-c.md")
score_register_license(KFR URL "https://github.com/kfrlib/kfr" FILES "${_l}/kfr/LICENSE.txt")
score_register_license(libe131 URL "https://github.com/hhromic/libe131" FILES "${_l}/libe131/LICENSE")
score_register_license(libremidi URL "https://github.com/celtera/libremidi" FILES "${_l}/libremidi/LICENSE.md")
score_register_license(libsamplerate URL "https://github.com/libsndfile/libsamplerate" FILES "${_l}/libsamplerate/COPYING")
score_register_license(libsndfile URL "https://github.com/libsndfile/libsndfile" FILES "${_t}/libsndfile/COPYING")
score_register_license(LLFIO URL "https://github.com/ned14/llfio" FILES "${_t}/llfio/Licence.txt")
score_register_license(magic_enum URL "https://github.com/Neargye/magic_enum" FILES "${_l}/magic_enum/LICENSE")
score_register_license(mdspan URL "https://github.com/kokkos/mdspan" FILES "${_l}/mdspan/LICENSE")
score_register_license(miniz URL "https://github.com/richgel999/miniz" FILES "${_t}/zipdownloader/LICENSE.miniz")
score_register_license(nano-signal-slot URL "https://github.com/jcelerier/nano-signal-slot" FILES "${_l}/nano-signal-slot/LICENSE")
score_register_license(OSCPack URL "https://github.com/jcelerier/oscpack" FILES "${_l}/oscpack/LICENSE")
score_register_license(Outcome URL "https://github.com/ned14/outcome" FILES "${_t}/outcome/Licence.txt")
score_register_license(PerlinNoise URL "https://github.com/Reputeless/PerlinNoise" FILES "${_l}/PerlinNoise/LICENSE")
score_register_license(phantomstyle URL "https://github.com/randrew/phantomstyle" FILES "${_t}/phantomstyle/LICENSE")
score_register_license(QCodeEditor URL "https://github.com/Megaxela/QCodeEditor" FILES "${_t}/QCodeEditor/LICENSE.MIT")
score_register_license(QMenuView URL "https://github.com/pvanek/qlipper/blob/master/qmenuview" NOTICES "${_t}/QMenuView/qmenuview.h")
score_register_license(QProgressIndicator URL "https://github.com/jcelerier/QProgressIndicator" FILES "${_t}/QProgressIndicator/LICENSE")
score_register_license(Qt-Color-Widgets URL "https://github.com/jcelerier/Qt-Color-Widgets" FILES "${_t}/Qt-Color-Widgets/COPYING")
score_register_license(QuickCppLib URL "https://github.com/ned14/quickcpplib" FILES "${_t}/quickcpplib/Licence.txt")
score_register_license(r8brain URL "https://github.com/avaneev/r8brain-free-src" FILES "${_l}/r8brain-free-src/LICENSE")
score_register_license(rapidfuzz URL "https://github.com/rapidfuzz/rapidfuzz-cpp" FILES "${_l}/rapidfuzz-cpp/LICENSE")
score_register_license(rapidhash URL "https://github.com/Nicoshev/rapidhash" FILES "${_l}/rapidhash/LICENSE")
score_register_license(rapidjson URL "https://github.com/Tencent/rapidjson" FILES "${_l}/rapidjson/license.txt")
score_register_license(ReaderWriterQueue URL "https://github.com/cameron314/readerwriterqueue" FILES "${_l}/readerwriterqueue/LICENSE.md")
score_register_license(re2 URL "https://github.com/google/re2" FILES "${_l}/re2/LICENSE")
score_register_license(rnd URL "https://github.com/jcelerier/rnd" FILES "${_l}/rnd/LICENSE")
score_register_license(Rubberband URL "https://github.com/breakfastquay/rubberband" FILES "${_l}/rubberband/COPYING")
score_register_license(SmallFunction URL "https://github.com/jcelerier/SmallFunction" HEADER "Based on https://github.com/LoopPerfect/smallfunction" FILES "${_l}/SmallFunction/LICENSE")
score_register_license(span URL "https://github.com/tcbrindle/span" FILES "${_l}/span/LICENSE_1_0.txt")
score_register_license(spdlog URL "https://github.com/gabime/spdlog" FILES "${_l}/spdlog/LICENSE")
score_register_license(TinySpline URL "https://github.com/msteinbeck/tinyspline" HEADER "MIT License")
score_register_license(tuplet URL "https://github.com/jcelerier/tuplet" FILES "${_l}/tuplet/LICENSE")
score_register_license(unordered_dense URL "https://github.com/martinus/unordered_dense" FILES "${_l}/unordered_dense/LICENSE")
score_register_license(verdigris URL "https://github.com/jcelerier/verdigris" FILES "${_l}/verdigris/LICENSE.LGPLv3")
score_register_license(weakjack URL "https://github.com/jcelerier/weakjack" HEADER "Based on https://github.com/x42/weakjack" NOTICES "${_l}/weakjack/weak_libjack.h")
score_register_license(websocketpp URL "https://github.com/jcelerier/websocketpp" FILES "${_l}/websocketpp/COPYING")
score_register_license(whereami URL "https://github.com/gpakosz/whereami" FILES "${_l}/whereami/LICENSE.MIT")
score_register_license(xsimd URL "https://github.com/xtensor-stack/xsimd" FILES "${_t}/xsimd/LICENSE")
score_register_license(xtensor URL "https://github.com/xtensor-stack/xtensor" FILES "${_t}/xtensor/LICENSE")
score_register_license(xtl URL "https://github.com/xtensor-stack/xtl" FILES "${_t}/xtl/LICENSE")
score_register_license(zipdownloader URL "https://github.com/jcelerier/zipdownloader" FILES "${_t}/zipdownloader/LICENSE")

if(OSSIA_DNSSD)
  score_register_license(Servus URL "https://github.com/jcelerier/Servus" HEADER "Based on https://github.com/HBPVIS/Servus"
    FILES "${_l}/Servus/LICENSE.txt" "${_l}/verdigris/LICENSE.LGPLv3")
  score_register_license(DNS-SD FILES "${_l}/Servus/3rdparty/dnssd/LICENSE")
endif()
if(OSSIA_PROTOCOL_WIIMOTE)
  score_register_license(wiiuse URL "https://github.com/ossia/wiiuse" FILES "${_l}/wiiuse/LICENSE")
endif()
if(OSSIA_PROTOCOL_MQTT5)
  score_register_license(async-mqtt5 URL "https://github.com/mireo/async-mqtt5" FILES "${_l}/async-mqtt5/LICENSE")
endif()
if(OSSIA_PROTOCOL_COAP)
  score_register_license(libcoap URL "https://libcoap.net" FILES "${_l}/libcoap/COPYING" "${_l}/libcoap/LICENSE")
endif()
if(TARGET shmdata)
  score_register_license(shmdata URL "https://gitlab.com/sat-mtl/tools/shmdata" FILES "${_t}/shmdata/LICENSE.md" "${_t}/libsndfile/COPYING")
endif()
if(TARGET sh4lt)
  score_register_license(sh4lt URL "https://gitlab.com/sh4lt/sh4lt" FILES "${_t}/sh4lt/LICENSE")
endif()
if(TARGET snappy OR TARGET Snappy::snappy)
  score_register_license(snappy URL "https://github.com/google/snappy" FILES "${_t}/snappy/COPYING")
endif()
if(TARGET simpleio)
  score_register_license(libsimpleio URL "https://github.com/pmunts/libsimpleio" NOTICES "${_t}/libsimpleio/libsimpleio/libgpio.h")
endif()

score_register_license(SDL URL "https://libsdl.org" HEADER "zlib License")
score_register_license(libJACK URL "https://jackaudio.org" HEADER "GNU Lesser General Public License")
score_register_license(Phidgets URL "https://www.phidgets.com/docs/Software_License" HEADER "Phidget22 library: see the Phidgets software license")
score_register_license(FFMPEG URL "https://ffmpeg.org" HEADER "GNU General Public License v3")
score_register_license(libusb URL "https://libusb.info/" HEADER "GNU Lesser General Public License v2.1 or any later version")
score_register_license(VST URL "https://steinberg.net"
  HEADER "VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.")
if(WIN32)
  score_register_license(ASIO URL "https://steinberg.net"
    HEADER "ASIO is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries."
    FILES "${_l}/asio/LICENSE.txt")
endif()
