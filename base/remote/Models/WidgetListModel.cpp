// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "WidgetListModel.hpp"
namespace RemoteUI
{
WidgetListData::~WidgetListData()
{
}

WidgetListModel::WidgetListModel(QQmlApplicationEngine& engine)
{
  componentList.push_back(new RemoteUI::WidgetListData{
      WidgetKind::HSlider, "HSlider", "Horizontal Slider",
      QUrl("qrc:///qml/widgets/dynamic/DynamicHSlider.qml"),
      QUrl("qrc:///qml/widgets/static/StaticSlider.qml"), engine});

  componentList.push_back(new RemoteUI::WidgetListData{
      WidgetKind::VSlider, "VSlider", "Vertical Slider",
      QUrl("qrc:///qml/widgets/dynamic/DynamicVSlider.qml"),
      QUrl("qrc:///qml/widgets/static/StaticSlider.qml"), engine});

  componentList.push_back(new RemoteUI::WidgetListData{
      WidgetKind::CheckBox, "CheckBox", "CheckBox",
      QUrl("qrc:///qml/widgets/dynamic/DynamicSwitch.qml"),
      QUrl("qrc:///qml/widgets/static/StaticSwitch.qml"), engine});

  componentList.push_back(new RemoteUI::WidgetListData{
      WidgetKind::LineEdit, "LineEdit", "LineEdit",
      QUrl("qrc:///qml/widgets/dynamic/DynamicLineEdit.qml"),
      QUrl("qrc:///qml/widgets/static/StaticLineEdit.qml"), engine});

  componentList.push_back(new RemoteUI::WidgetListData{
      WidgetKind::Label, "Label", "Label",
      QUrl("qrc:///qml/widgets/dynamic/DynamicLabel.qml"),
      QUrl("qrc:///qml/widgets/static/StaticLabel.qml"), engine});

  componentList.push_back(new RemoteUI::WidgetListData{
      WidgetKind::PushButton, "Button", "Button",
      QUrl("qrc:///qml/widgets/dynamic/DynamicButton.qml"),
      QUrl("qrc:///qml/widgets/static/StaticButton.qml"), engine});

  // QML absolutely wants a QList<QObject*>
  for (auto l : componentList)
    objectList.push_back(l);
}
}
