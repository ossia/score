#include "WidgetListModel.hpp"
namespace RemoteUI
{
WidgetListData::~WidgetListData()
{

}

WidgetListModel::WidgetListModel(QQmlApplicationEngine& engine)
{
  componentList.push_back(
        new RemoteUI::WidgetListData{
          WidgetKind::HSlider, "HSlider", "Horizontal Slider",
          QUrl("qrc:///qml/widgets/HSlider.qml"),
          QUrl("qrc:///qml/widgets/HSlider.qml"),
          engine
        });
  componentList.push_back(
        new RemoteUI::WidgetListData{
          WidgetKind::VSlider, "VSlider", "Vertical Slider",
          QUrl("qrc:///qml/widgets/VSlider.ui.qml"),
          QUrl("qrc:///qml/widgets/HSlider.qml"),
          engine
        });
  componentList.push_back(
        new RemoteUI::WidgetListData{
          WidgetKind::CheckBox, "CheckBox", "CheckBox",
          QUrl("qrc:///qml/widgets/UICheckBox.ui.qml"),
          QUrl("qrc:///qml/widgets/UICheckBox.ui.qml"),
          engine
        });
  componentList.push_back(
        new RemoteUI::WidgetListData{
          WidgetKind::LineEdit, "LineEdit", "LineEdit",
          QUrl("qrc:///qml/widgets/UILineEdit.ui.qml"),
          QUrl("qrc:///qml/widgets/UILineEdit.ui.qml"),
          engine
        });
  componentList.push_back(
        new RemoteUI::WidgetListData{
          WidgetKind::Label, "Label", "Label",
          QUrl("qrc:///qml/widgets/TextLabel.ui.qml"),
          QUrl("qrc:///qml/widgets/TextLabel.ui.qml"),
          engine
        });
  componentList.push_back(
        new RemoteUI::WidgetListData{
          WidgetKind::PushButton, "Button", "Button",
          QUrl("qrc:///qml/widgets/UIButton.ui.qml"),
          QUrl("qrc:///qml/widgets/UIButton.ui.qml"),
          engine
        });

  // QML absolutely wants a QList<QObject*>
  for(auto l : componentList)
    objectList.push_back(l);

}
}
