#-------------------------------------------------
#
# Project created by QtCreator 2014-11-17T11:31:48
#
#-------------------------------------------------

QT       += core gui

CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Inspector
TEMPLATE = app


SOURCES += main.cpp\
    InspectorPanel.cpp \
    InspectorSectionWidget.cpp \
    InspectorWidgetInterface.cpp \
    IntervalInspectorview.cpp \
    InspectorWidget.cpp \
    objectinterval.cpp \
    ReorderWidget.cpp

HEADERS  += InspectorPanel.hpp \
    InspectorSectionWidget.hpp \
    InspectorWidgetInterface.hpp \
    IntervalInspectorview.hpp \
    InspectorWidgetFactoryInterface.hpp \
    InspectorWidget.hpp \
    objectinterval.hpp \
    ReorderWidget.hpp

FORMS    += InspectorPanel.ui

OTHER_FILES += \
    Inspector.pro.user
