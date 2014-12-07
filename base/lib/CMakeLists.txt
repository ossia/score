cmake_minimum_required(VERSION 2.8)
project(iscore_core)

# Configuration for the subprojects
set(ISCORE_PLUGINS_FOLDER "${CMAKE_BINARY_DIR}/plugins")

# Packages
# Note : if building with a Qt installed in e.g. /home/myuser/Qt/ or /Users/Qt or c:\Qt\
# keep in mind that you have to call CMake with :
# $ cmake -DCMAKE_MODULE_PATH={path/to/qt/5.3}/{gcc64,clang,msvc2013...}/lib/cmake/Qt5
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)

# Files & main target
include_directories(.)
include_directories(core)
include_directories("${CMAKE_SOURCE_DIR}")

set(HEADER_FILES
### interface ###
	${CMAKE_CURRENT_SOURCE_DIR}/interface/autoconnect/Autoconnect.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/library/LibraryElement.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/inspector/InspectorWidgetFactoryInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/inspector/InspectorWidgetBase.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/inspector/InspectorSectionWidget.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugincontrol/PluginControlInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugincontrol/MenuInterface.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/panel/PanelFactoryInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/panel/PanelModelInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/panel/PanelViewInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/panel/PanelPresenterInterface.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/documentdelegate/DocumentDelegateModelInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/documentdelegate/DocumentDelegateFactoryInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/documentdelegate/DocumentDelegatePresenterInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/documentdelegate/DocumentDelegateViewInterface.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/process/ProcessFactoryInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/process/ProcessSharedModelInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/process/ProcessViewModelInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/process/ProcessViewInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/process/ProcessPresenterInterface.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/settingsdelegate/SettingsDelegateModelInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/settingsdelegate/SettingsDelegatePresenterInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/settingsdelegate/SettingsDelegateViewInterface.hpp

### interface/plugins ###
#todo
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/LibraryElementFactoryPluginInterface.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/PanelFactoryInterface_QtInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/PluginControlInterface_QtInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/ProcessFactoryInterface_QtInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/SettingsDelegateFactoryInterface_QtInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/Autoconnect_QtInterface.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugins/InspectorWidgetFactoryInterface_QtInterface.hpp

### core ###
	${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/QNamedObject

	${CMAKE_CURRENT_SOURCE_DIR}/core/application/ChildEventFilter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/application/Application.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/processes/ProcessList.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/model/Model.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Action.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/MenubarManager.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/command/CommandQueue.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/command/Command.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/command/SerializableCommand.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/view/View.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsPresenter.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentPresenter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentModel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentView.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/document/Document.hpp



)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/interface/plugincontrol/MenuInterface.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/inspector/InspectorWidgetBase.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/interface/inspector/InspectorSectionWidget.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/application/Application.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/processes/ProcessList.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentPresenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/document/DocumentView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/document/Document.cpp

# MVP
	${CMAKE_CURRENT_SOURCE_DIR}/core/model/Model.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/MenubarManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/Presenter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/command/CommandQueue.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/presenter/command/SerializableCommand.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/core/view/View.cpp

# Settings
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/Settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsModel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/settings/SettingsPresenter.cpp
)

add_library(iscore_lib ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(iscore_lib Qt5::Core Qt5::Widgets)

install(TARGETS iscore_lib DESTINATION ${CMAKE_BINARY_DIR})
