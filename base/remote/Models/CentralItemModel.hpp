#ifndef CENTRALITEMMODEL_H
#define CENTRALITEMMODEL_H

#include <QObject>
#include <QQuickItem>
#include <Models/WidgetListModel.hpp>
#include <Device/Address/AddressSettings.hpp>
class QQmlApplicationEngine;

namespace Device
{
class FullAddressSettings;
}
namespace RemoteUI
{
class WebSocketClient;
class NodeModel;
class WidgetListData;
struct Context
{
  Context(
      QQmlApplicationEngine& e,
      const QList<RemoteUI::WidgetListData*>& w,
      const NodeModel& n,
      WebSocketClient& ws,
      QQuickItem* i):
    engine{e},
    widgets{w},
    nodes{n},
    websocket{ws},
    centralItem{i}
  {

  }
  QQmlApplicationEngine& engine;
  const QList<RemoteUI::WidgetListData*>& widgets;
  const NodeModel& nodes;
  WebSocketClient& websocket;
  QQuickItem* centralItem{};

};


class GUIItem : public QObject
{
  Q_OBJECT
  friend class SetSliderAddress;
public:
  GUIItem(Context& ctx, WidgetKind c, QQuickItem* it);

  auto item() const { return m_item; }
  void setAddress(const Device::FullAddressSettings&);

private slots:
  void on_impulse();
  void on_boolValueChanged(bool);
  void on_intValueChanged(qreal);
  void on_floatValueChanged(qreal);
  void on_stringValueChanged(QString);
  void sendMessage(const State::Message& m);

private:
  Context& m_context;

  WidgetKind m_compType;
  QQuickItem* m_item;
  Device::FullAddressSettings m_addr;
  QMetaObject::Connection m_connection;
};

class CentralItemModel : public QObject
{
  Q_OBJECT
public:
  explicit CentralItemModel(Context&, QObject *parent = 0);

public slots:
  void on_itemCreated(QString data, qreal x, qreal y);
  void on_addressCreated(QString data, qreal x, qreal y);

private:
  Context& m_ctx;
  QList<GUIItem*> m_guiItems;
  QQuickItem* create(WidgetKind c);
};

}
#endif // CENTRALITEMMODEL_H
