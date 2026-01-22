#include "DeviceContext.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <JS/Executor/JSAPIWrapper.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/network/base/parameter.hpp>

#include <ossia-qt/qml_engine_functions.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::DeviceContext)

namespace JS
{
DeviceContext::DeviceContext(QQmlEngine& engine, QObject* parent)
    : QObject{parent}
    , m_engine{engine}
{
}

DeviceContext::~DeviceContext() { }

bool DeviceContext::init()
{
  if(!m_impl)
  {
    [[unlikely]];

    auto& ctx = score::GUIAppContext();
    auto doc = ctx.currentDocument();
    if(!doc)
      return false;

    auto m_devices = doc->findPlugin<Explorer::DeviceDocumentPlugin>();
    if(!m_devices)
      return false;

    ossia::qt::qml_device_cache cache;
    auto& dlist = m_devices->list();
    m_devices->list().apply([&cache](Device::DeviceInterface& iface) {
      if(auto ossia = iface.getDevice())
        cache.push_back(ossia);
    });

    m_impl = new ossia::qt::qml_engine_functions{
        cache, [](ossia::net::parameter_base& param, const ossia::value_port& v) {
      if(v.get_data().empty())
        return;
      auto& last = v.get_data().back().value;
      param.push_value(last);
    }, m_engine, this};

    connect(this, &DeviceContext::exec,
            m_impl, &ossia::qt::qml_engine_functions::exec);
    connect(this, &DeviceContext::compute,
            m_impl, &ossia::qt::qml_engine_functions::compute);
    connect(this, &DeviceContext::system,
            m_impl, &ossia::qt::qml_engine_functions::system);

    connect(&dlist, &Device::DeviceList::deviceAdded,
            this, [this, p = QPointer{m_impl}] (Device::DeviceInterface* iface){
      if(!p)
        return;

      if(auto ossia = iface->getDevice())
        m_impl->devices.push_back(ossia);
      connect(iface, &Device::DeviceInterface::deviceChanged,
              this, [this, p] (ossia::net::device_base* old_dev, ossia::net::device_base* new_dev) {
        if(auto it = ossia::find(m_impl->devices, old_dev); it != m_impl->devices.end()) {
          m_impl->devices.erase(it);
        }

        if(new_dev) {
          m_impl->devices.push_back(new_dev);
        }

        m_impl->clearCache();
      });
    }, Qt::QueuedConnection);

    connect(&dlist, &Device::DeviceList::deviceRemoved,
            this, [this, p = QPointer{m_impl}] (Device::DeviceInterface* iface){
      if(!p)
        return;

      if(auto o = iface->getDevice()) {
        if(auto it = ossia::find(m_impl->devices, o); it != m_impl->devices.end()) {
          m_impl->devices.erase(it);
          m_impl->clearCache();
        }
      }
    });
  }
  return true;
}

QVariant DeviceContext::read(const QString& address)
{
  if(!init())
  {
    [[unlikely]];
    return {};
  }
  return m_impl->read(address);
}

void DeviceContext::write(const QString& address, const QVariant& value)
{
  if(!init())
  {
    [[unlikely]];
    return;
  }
  return m_impl->write(address, value);
}

ossia::net::node_base* DeviceContext::find(const QString& addr)
{
  if(!init())
  {
    [[unlikely]];
    return nullptr;
  }
  auto res = m_impl->find_address(addr);
  if(auto p = res.target<ossia::net::parameter_base*>())
  {
    return &(*p)->get_node();
  }
  else if(auto n = res.target<ossia::net::node_base*>())
  {
    return *n;
  }
  else
  {
    return nullptr;
  }
}

QVariant DeviceContext::asArray(QVariant v) const noexcept
{
  return m_impl->asArray(v);
}
QVariant DeviceContext::asColor(QVariant v) const noexcept
{
  return m_impl->asColor(v);
}
QVariant DeviceContext::asVec2(QVariant v) const noexcept
{
  return m_impl->asVec2(v);
}
QVariant DeviceContext::asVec3(QVariant v) const noexcept
{
 return m_impl->asVec3(v);
}
QVariant DeviceContext::asVec4(QVariant v) const noexcept
{
  return m_impl->asVec4(v);
}

}
