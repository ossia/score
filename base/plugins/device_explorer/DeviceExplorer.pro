TEMPLATE = app
TARGET = DeviceExplorer

CONFIG += debug_and_release

QT += core widgets

RESOURCES += DeviceExplorer.qrc

COMMAND_DIRECTORY=../../base/lib/core/presenter/  

INCLUDEPATH += $$COMMAND_DIRECTORY ../../base/lib/

QMAKE_CXXFLAGS += -std=c++11 -Wall -Wextra

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
    CONFIG += x86_64
    QMAKE_CXXFLAGS += -stdlib=libc++ -std=c++11
    QMAKE_LFLAGS += -mmacosx-version-min=$$QMAKE_MACOSX_DEPLOYMENT_TARGET
    QMAKE_LFLAGS += -stdlib=libc++ -std=c++11
}

linux-clang{
	QMAKE_CXXFLAGS += -stdlib=libc++
	QMAKE_LFLAGS += -stdlib=libc++
}

HEADERS += \
MainWindow.hpp \
DeviceExplorerModel.hpp \
DeviceExplorerView.hpp \
DeviceExplorerWidget.hpp \
DeviceExplorerFilterProxyModel.hpp \
DeviceExplorerMoveCommand.hpp \
DeviceExplorerInsertCommand.hpp \
DeviceExplorerRemoveCommand.hpp \
DeviceExplorerCutCommand.hpp \
DeviceExplorerPasteCommand.hpp \
IOTypeDelegate.hpp \
NodeFactory.hpp \
DeviceEditDialog.hpp \
MIDIProtocolSettingsWidget.hpp \
MinuitProtocolSettingsWidget.hpp \
OSCProtocolSettingsWidget.hpp \
AddressEditDialog.hpp \
AddressFloatSettingsWidget.hpp \
AddressIntSettingsWidget.hpp \
AddressStringSettingsWidget.hpp \
$$COMMAND_DIRECTORY\command\Command.hpp \
$$COMMAND_DIRECTORY\command\CommandQueue.hpp \
$$COMMAND_DIRECTORY\command\SerializableCommand.hpp 

SOURCES += \
main.cpp \
MainWindow.cpp \
DeviceExplorerModel.cpp \
DeviceExplorerView.cpp \
DeviceExplorerWidget.cpp \
DeviceExplorerFilterProxyModel.cpp \
DeviceExplorerMoveCommand.cpp \
DeviceExplorerInsertCommand.cpp \
DeviceExplorerRemoveCommand.cpp \
DeviceExplorerCutCommand.cpp \
DeviceExplorerPasteCommand.cpp \
IOTypeDelegate.cpp \
NodeFactory.cpp \
DeviceEditDialog.cpp \
MIDIProtocolSettingsWidget.cpp \
MinuitProtocolSettingsWidget.cpp \
OSCProtocolSettingsWidget.cpp  \
AddressEditDialog.cpp \
AddressFloatSettingsWidget.cpp \
AddressIntSettingsWidget.cpp \
AddressStringSettingsWidget.cpp \
$$COMMAND_DIRECTORY\command\Command.cpp \
$$COMMAND_DIRECTORY\command\CommandQueue.cpp \
$$COMMAND_DIRECTORY\command\SerializableCommand.cpp 


debug {
  exists(ModelTest/modeltest.pri) {
    DEFINES += MODEL_TEST
    include(ModelTest/modeltest.pri)
  }
}
