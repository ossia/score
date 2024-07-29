#pragma once
#include <ossia/detail/callback_container.hpp>
#include <ossia/network/base/value_callback.hpp>

#include <QQmlProperty>
#include <QQmlPropertyValueSource>
#include <QQuickItem>

#include <nano_observer.hpp>

#include <verdigris>
namespace ossia::net
{
class device_base;
class parameter_base;
}
namespace JS
{
class DeviceContext;
struct AddressItem : public QQuickItem
{
  W_OBJECT(AddressItem)
public:
  explicit AddressItem(QQuickItem* parent = nullptr);
  ~AddressItem();

  void on_addressChanged(const QString& addr);

  INLINE_PROPERTY_CREF(QString, address, = "", address, setAddress, addressChanged)

private:
  JS::DeviceContext* m_devices{};
};

struct AddressSource
    : public QObject
    , public QQmlPropertyValueSource
    , public Nano::Observer
{
  W_OBJECT(AddressSource)
  W_INTERFACE(QQmlPropertyValueSource)
  QML_ELEMENT

public:
  explicit AddressSource(QObject* parent = nullptr);
  ~AddressSource();

  void init();

  void setTarget(const QQmlProperty& prop) override;

  void clearNetworkCallbacks();
  void clearQMLCallbacks();
  void on_addressChanged(const QString& addr);
  void on_newNetworkValue(const ossia::value& v);
  void on_newUIValue();
  W_SLOT(on_newUIValue);

  void on_parameterRemoving(const ossia::net::parameter_base& v);

  INLINE_PROPERTY_CREF(QString, address, = "", address, setAddress, addressChanged)

  // Send updates from the GUI to the network
  INLINE_PROPERTY_CREF(
      bool, sendUpdates, = true, sendUpdates, setSendUpdates, sendUpdatesChanged)

  // Receive updates from the network to the GUI
  INLINE_PROPERTY_CREF(
      bool, receiveUpdates, = true, receiveUpdates, setReceiveUpdates,
      receiveUpdatesChanged)

private:
  void rebuild();

  QQmlProperty m_targetProperty;
  JS::DeviceContext* m_devices{};

  ossia::net::device_base* m_device{};
  ossia::net::parameter_base* m_param{};
  std::optional<ossia::callback_container<ossia::value_callback>::iterator> m_callback;
  bool m_writingValue{};
};
}
