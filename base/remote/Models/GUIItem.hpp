#pragma once
#include <WidgetKind.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <QObject>
namespace State
{
class Message;
}
class QQuickItem;
namespace Device
{
class FullAddressSettings;
}
namespace RemoteUI
{
class Context;

class GUIItem : public QObject
{
  Q_OBJECT
  friend class SetSliderAddress;
  friend class SetCheckboxAddress;
  friend class SetLineEditAddress;

public:
  GUIItem(Context& ctx, WidgetKind c, QQuickItem* it);

  auto item() const { return m_item; }
  void setAddress(const Device::FullAddressSettings&);

private slots:
  void setAddress(QString);

  void on_impulse();
  void on_boolValueChanged(bool);
  void on_intValueChanged(qreal);
  void on_floatValueChanged(qreal);
  void on_stringValueChanged(QString);
  void on_parsableValueChanged(QString);

private:
  void sendMessage(const State::Message& m);
  Context& m_ctx;

  WidgetKind m_compType;
  QQuickItem* m_item;
  Device::FullAddressSettings m_addr;
  QMetaObject::Connection m_connection;
};

}
