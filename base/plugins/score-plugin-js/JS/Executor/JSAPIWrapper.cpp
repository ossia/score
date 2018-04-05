// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include "JSAPIWrapper.hpp"
#include <ossia/network/value/value.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Engine/score2OSSIA.hpp>
#include <ossia/detail/apply.hpp>
#include <ossia-qt/js_utilities.hpp>

namespace JS
{

ExecStateWrapper::~ExecStateWrapper()
{

}

ossia::net::parameter_base*ExecStateWrapper::find_address(const QString& str)
{
  auto it = m_address_cache.find(str);
  if(it != m_address_cache.end())
  {
    return it->second;
  }

  auto d = str.indexOf(':');
  if(d == -1)
  {
    // Address looks like '/foo/bar'
    // Try to find automatically in all devices
    auto node = devices.find_node(str.toStdString());
    if(node)
    {
      if(auto addr = node->get_parameter())
      {
        m_address_cache.insert({str, addr});
        return addr;
      }
    }
  }


  // Split in devices
  auto dev = ossia::find_if(
               devices.allDevices,
               [=,devname=str.mid(0, d).toStdString()] (const auto& dev) {
    return dev->get_name() == devname;
  } );

  if(dev != devices.allDevices.end())
  {
    if(d == str.size() - 1)
    {
      if(auto addr = (*dev)->get_root_node().get_parameter())
      {
        m_address_cache.insert({str,addr});
        return addr;
      }
    }

    auto node = ossia::net::find_node((*dev)->get_root_node(), str.mid(d + 1).toStdString());
    if(node)
    {
      if(auto addr = node->get_parameter())
      {
        m_address_cache.insert({str, addr});
        return addr;
      }
    }
  }

  return nullptr;
}

QVariant ExecStateWrapper::read(const QString& address)
{
  if(auto addr = find_address(address))
  {
    return addr->value().apply(ossia::qt::ossia_to_qvariant{});
  }
  return {};
}

void ExecStateWrapper::write(const QString& address, const QVariant& value)
{
  if(auto addr = find_address(address))
  {
    addr->push_value(ossia::qt::qt_to_ossia{}(value));
  }
}

}
