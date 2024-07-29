#include "DeviceContext.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <JS/Executor/JSAPIWrapper.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/network/base/parameter.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::DeviceContext)

namespace JS
{
DeviceContext::DeviceContext(QObject* parent)
    : QObject{parent}
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

    DeviceCache cache;
    m_devices->list().apply([&cache](Device::DeviceInterface& iface) {
      if(auto ossia = iface.getDevice())
        cache.push_back(ossia);
    });

    m_impl = new ExecStateWrapper{
        cache, [](ossia::net::parameter_base& param, const ossia::value_port& v) {
      if(v.get_data().empty())
        return;
      auto& last = v.get_data().back().value;
      param.push_value(last);
    }, this};
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
}
