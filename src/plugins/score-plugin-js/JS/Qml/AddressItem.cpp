#include "AddressItem.hpp"

#include <JS/Qml/DeviceContext.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/js_utilities.hpp>

#include <QQmlContext>
#include <QQmlEngine>

#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::AddressItem)
W_OBJECT_IMPL(JS::AddressSource)

namespace JS
{

AddressItem::AddressItem(QQuickItem* parent)
    : QQuickItem{parent}
{
  connect(this, &QQuickItem::parentChanged, this, [this](QQuickItem* parent) {
    if(!parent)
      return;
    auto ctx = qmlContext(parent);
    m_devices = qobject_cast<JS::DeviceContext*>(
        ctx->engine()->globalObject().property("Device").toQObject());
    on_addressChanged(m_address);
  });

  connect(this, &AddressItem::addressChanged, this, &AddressItem::on_addressChanged);
}

void AddressItem::on_addressChanged(const QString& addr)
{
  if(!m_devices)
    return;
  auto node = m_devices->find(addr);
  if(!node)
    return;
  auto param = node->get_parameter();
  if(!param)
    return;
  switch(param->get_value_type())
  {
    case ossia::val_type::FLOAT:
      break;
  }
}

AddressItem::~AddressItem() { }

AddressSource::AddressSource(QObject* parent)
    : QObject{parent}
{
  connect(this, &AddressSource::addressChanged, this, &AddressSource::on_addressChanged);
  connect(this, &AddressSource::sendUpdatesChanged, this, [this] {
    clearQMLCallbacks();
    rebuild();
  });
  connect(this, &AddressSource::receiveUpdatesChanged, this, [this] {
    on_addressChanged(address());
  });
}

AddressSource::~AddressSource()
{
  clearNetworkCallbacks();
}

void AddressSource::init()
{
  if(m_devices)
    return;
  if(!parent())
    return;
  auto ctx = qmlContext(parent());
  m_devices = qobject_cast<JS::DeviceContext*>(
      ctx->engine()->globalObject().property("Device").toQObject());
}

void AddressSource::on_addressChanged(const QString& addr)
{
  if(!m_devices)
  {
    init();
    if(!m_devices)
      return;
  }

  clearNetworkCallbacks();

  // Set new callback
  // First find the node
  auto node = m_devices->find(addr);
  if(!node)
    return;
  m_param = node->get_parameter();
  if(!m_param)
    return;

  // Connect to it
  if(m_receiveUpdates)
  {
    m_callback = m_param->add_callback([ptr = QPointer{this}](const ossia::value& v) {
      if(!ptr)
        return;

      ossia::qt::run_async(QCoreApplication::instance(), [ptr, v]() mutable {
        if(ptr)
        {
          ptr->on_newNetworkValue(std::move(v));
        }
      });
    });
  }

  // Get a notification if the node gets removed
  m_device = &node->get_device();
  m_device->on_parameter_removing.connect<&AddressSource::on_parameterRemoving>(this);
}

void AddressSource::on_parameterRemoving(const ossia::net::parameter_base& v)
{
  clearNetworkCallbacks();
}

void AddressSource::on_newUIValue()
{
  // Avoir signal loops
  if(m_writingValue)
    return;
  if(m_param)
  {
    m_param->push_value(ossia::qt::qt_to_ossia{}(m_targetProperty.read()));
  }
}

void AddressSource::on_newNetworkValue(const ossia::value& v)
{
  auto vv = v.apply(ossia::qt::ossia_to_qvariant{});
  m_writingValue = true;
  m_targetProperty.write(vv);
  m_writingValue = false;
}

void AddressSource::setTarget(const QQmlProperty& prop)
{
  clearQMLCallbacks();

  m_targetProperty = prop;

  rebuild();
}

void AddressSource::clearQMLCallbacks()
{
  if(auto obj = m_targetProperty.object())
    obj->disconnect(this);
}

void AddressSource::rebuild()
{
  if(m_sendUpdates)
  {
    if(m_targetProperty.hasNotifySignal())
    {
      m_targetProperty.connectNotifySignal(this, SLOT(on_newUIValue()));
    }
  }
  on_addressChanged(m_address);
}

void AddressSource::clearNetworkCallbacks()
{
  // Remove old callback
  if(m_callback && m_device && m_param)
  {
    m_param->remove_callback(*m_callback);
    m_callback.reset();

    m_device->on_parameter_removing.disconnect<&AddressSource::on_parameterRemoving>(
        *this);
  }

  m_device = nullptr;
  m_param = nullptr;
}
}
