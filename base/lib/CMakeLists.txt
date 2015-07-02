cmake_minimum_required(VERSION 3.0)
project(iscore_lib_base)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets )

if(NOT Boost_FOUND)
        message("i-score requires boost to build")
        return()
endif()

# Files & main target
include_directories(.)

file(GLOB_RECURSE HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iscore/*.hpp)

set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/iscore/menu/MenuInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/ObjectPathSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore/tools/SettableIdentifierSerialization.cpp"
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

"${CMAKE_CURRENT_SOURCE_DIR}/core/application/Application.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/application/OpenDocumentsFile.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/command/CommandStack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentView.cpp"
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

add_library(iscore_lib_base ${SRCS} ${HEADERS})
target_link_libraries(iscore_lib_base Qt5::Core Qt5::Widgets)

cotire(iscore_lib_base)
set_target_properties(iscore_lib_base
                      PROPERTIES
                        COTIRE_PREFIX_HEADER_IGNORE_PATH "")

install(TARGETS iscore_lib_base
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION .)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
