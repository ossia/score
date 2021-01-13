// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSAPIWrapper.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia-qt/js_utilities.hpp>
#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/dataflow.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/midi_port.hpp>
#include <ossia/dataflow/typed_value.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/apply.hpp>
#include <ossia/network/value/value.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::ExecStateWrapper)
namespace JS
{

ExecStateWrapper::~ExecStateWrapper() { }

const ossia::destination_t& ExecStateWrapper::find_address(const QString& str)
{
  // OPTIMIZEME this function can be optimized a lot
  // c.f. MapperDevice.cpp:find_parameter
  auto it = m_address_cache.find(str);
  if (it != m_address_cache.end())
  {
    return it->second;
  }

  auto d = str.indexOf(':');
  if (d == -1)
  {
    // Address looks like '/foo/bar'
    // Try to find automatically in all devices
    auto node = devices.find_node(str.toStdString());
    if (node)
    {
      if (auto addr = node->get_parameter())
      {
        auto [it, b] = m_address_cache.insert({str, addr});
        return it->second;
      }
    }
  }

  // Split in devices
  auto dev = ossia::find_if(
      devices.exec_devices(), [devname = str.mid(0, d).toStdString()](const auto& dev) {
        return dev->get_name() == devname;
      });

  if (dev != devices.exec_devices().end())
  {
    if (d == str.size() - 1)
    {
      if (auto addr = (*dev)->get_root_node().get_parameter())
      {
        auto [it, b] = m_address_cache.insert({str, addr});
        return it->second;
      }
    }

    auto node = ossia::net::find_node((*dev)->get_root_node(), str.mid(d + 1).toStdString());
    if (node)
    {
      if (auto addr = node->get_parameter())
      {
        auto [it, b] = m_address_cache.insert({str, addr});
        return it->second;
      }
    }
  }

  if (auto p = ossia::traversal::make_path(str.toStdString()))
  {
    auto [it, b] = m_address_cache.insert({str, *p});
    return it->second;
  }

  static ossia::destination_t bad_dest;
  return bad_dest;
}

QVariant ExecStateWrapper::read(const QString& address)
{
  if (auto addr = find_address(address))
  {
    QVariant var;
    QVariantMap mv;

    bool unique = ossia::apply_to_destination_impl(
        addr,
        devices.exec_devices(),
        [&](ossia::net::parameter_base* addr, bool unique) {
          if (unique)
          {
            var = addr->value().apply(ossia::qt::ossia_to_qvariant{});
          }
          else
          {
            mv[QString::fromStdString(addr->get_node().osc_address())]
                = addr->value().apply(ossia::qt::ossia_to_qvariant{});
          }
        },
        ossia::do_nothing_for_nodes{});
    if (unique)
      return var;
    else
      return mv;
  }
  return {};
}

void ExecStateWrapper::write(const QString& address, const QVariant& value)
{
  if (const auto& addr = find_address(address))
  {
    auto val = ossia::qt::qt_to_ossia{}(value);

    ossia::apply_to_destination_impl(
        addr,
        devices.exec_devices(),
        [&](ossia::net::parameter_base* addr, bool unique) {
          devices.insert(*addr, ossia::typed_value{val});
        },
        ossia::do_nothing_for_nodes{});
  }
}

}
