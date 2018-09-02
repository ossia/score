#include "ZeroconfListener.hpp"
#include <Models/NodeModel.hpp>
#include <ossia/network/oscquery/oscquery_mirror.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/common/node_visitor.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/resolver_service.hpp>

namespace RemoteUI
{

static Device::AddressSettings
ToAddressSettings(const ossia::net::node_base& node)
{
  Device::AddressSettings s;
  const auto& addr = node.get_parameter();

  if (addr)
  {
    addr->request_value();

    s.name = QString::fromStdString(node.get_name());
    s.ioType = ossia::access_mode::BI; // addr->get_access();
    s.clipMode = addr->get_bounding();
    s.repetitionFilter = addr->get_repetition_filter();
    s.unit = addr->get_unit();
    s.extendedAttributes = node.get_extended_attributes();
    s.domain = addr->get_domain();

    try
    {
      s.value = addr->value();
    }
    catch (...)
    {
      s.value = ossia::init_value(addr->get_value_type());
    }
  }
  else
  {
    s.name = QString::fromStdString(node.get_name());
  }
  return s;
}

Device::Node ToDeviceExplorer(const ossia::net::node_base& ossia_node)
{
  Device::Node score_node{ToAddressSettings(ossia_node), nullptr};
  {
    const auto& cld = ossia_node.children();
    score_node.reserve(cld.size());

    // 2. Recurse on the children
    for (const auto& ossia_child : cld)
    {
      if (!ossia::net::get_hidden(*ossia_child)
          && !ossia::net::get_zombie(*ossia_child))
      {
        auto child_n = ToDeviceExplorer(*ossia_child);
        child_n.setParent(&score_node);
        score_node.push_back(std::move(child_n));
      }
    }
  }
  return score_node;
}

ZeroconfListener::ZeroconfListener(Context& ctx)
  : context{ctx}
  , service{"_oscjson._tcp"}
{
  for(const auto& i : service.getInstances())
    instanceAdded(i);

  service.addListener(this);
  service.beginBrowsing(servus::Interface::IF_ALL);

  QObject::connect(&timer, &QTimer::timeout,
                   [this] { service.browse(0); });
  timer.start(100);
}

ZeroconfListener::~ZeroconfListener()
{
  timer.stop();
  service.removeListener(this);
  service.endBrowsing();
}

void ZeroconfListener::instanceAdded(const std::string& instance)
{
  for(const auto& dev : context.device)
  {
    qDebug() << dev->get_name().c_str() << instance.c_str();
    if(dev->get_name() == instance)
      return;
  }

  std::string ip = service.get(instance, "servus_ip");
  std::string port = service.get(instance, "servus_port");
  if(ip.empty())
  {
    ip = service.get(instance, "servus_host");
  }

  try
  {
    asio::io_service io_service;
    asio::ip::tcp::resolver resolver(io_service);
    asio::ip::tcp::resolver::query query(ip, port);
    asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
    if(iter->endpoint().address().is_loopback())
    {
      ip = "localhost";
    }
  }
  catch(...)
  {

  }

  if (ip.empty() || port.empty())
  {
    return;
  }

  auto clt = std::make_unique<ossia::net::generic_device>(
        std::make_unique<ossia::oscquery::oscquery_mirror_protocol>("ws://" + ip + ":" + port)
        , instance);
  clt->get_protocol().update(clt->get_root_node());
  auto n = ToDeviceExplorer(clt->get_root_node());
  n.set(Device::DeviceSettings{{}, QString::fromStdString(clt->get_name())});
  context.nodes.add_device(std::move(n));
  context.device.push_back(std::move(clt));
}

void ZeroconfListener::instanceRemoved(const std::string& instance)
{
  context.nodes.remove_device(QString::fromStdString(instance));
  auto it = ossia::find_if(context.device, [&] (const auto& d) {
    return d->get_name() == instance;
  });

  if(it != context.device.end())
  {
    context.device.erase(it);
  }
}
}
