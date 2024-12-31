// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/Bitfocus/BitfocusContext.hpp>
#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <memory>
namespace ossia::net
{
class bitfocus_protocol : public ossia::net::protocol_base
{
public:
  bitfocus_protocol(
      std::shared_ptr<bitfocus::module_handler> rc, ossia::net::network_context_ptr ctx)
      : m_rc{rc}
      , m_context{ctx}
  {
  }

  bool pull(ossia::net::parameter_base&) override { return true; }
  bool push(const ossia::net::parameter_base& p, const ossia::value& v) override
  {
    auto parent = p.get_node().get_parent();
    if(parent == nodes.actions)
    {
      QMetaObject::invokeMethod(m_rc.get(), [m = m_rc, name = p.get_node().get_name()] {
        m->actionRun(name);
      });
    }
    else if(parent == nodes.presets)
    {
    }
    else if(parent == nodes.feedbacks)
    {
    }
    return true;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return true; }
  bool observe(ossia::net::parameter_base&, bool) override { return true; }
  bool update(ossia::net::node_base& node_base) override { return true; }

  void set_device(ossia::net::device_base& dev) override
  {
    // Start creating the tree
    nodes.actions = dev.get_root_node().create_child("action");
    nodes.presets = dev.get_root_node().create_child("presets");
    nodes.feedbacks = dev.get_root_node().create_child("feedback");
    nodes.variables = dev.get_root_node().create_child("variable");

    for(auto& v : m_rc->model().actions)
    {
      auto node = nodes.actions->create_child(v.first.toStdString());
      ossia::net::set_description(*node, v.second.name.toStdString());
      auto param = node->create_parameter(ossia::val_type::IMPULSE);
    }

    for(auto& v : m_rc->model().presets)
    {
      auto node = nodes.presets->create_child(v.first.toStdString());
      ossia::net::set_description(*node, v.second.name.toStdString());
      auto param = node->create_parameter(ossia::val_type::IMPULSE);
    }

    for(auto& v : m_rc->model().feedbacks)
    {
      auto node = nodes.feedbacks->create_child(v.first.toStdString());
      ossia::net::set_description(*node, v.second.name.toStdString());
      auto param = node->create_parameter(ossia::val_type::IMPULSE);
    }

    for(auto& v : m_rc->model().variables)
    {
      auto node = nodes.variables->create_child(v.first.toStdString());
      ossia::net::set_description(*node, v.second.name.toStdString());
      auto val = ossia::qt::qt_to_ossia{}(v.second.value);

      if(val.get_type() != ossia::val_type::NONE)
      {
        auto param = node->create_parameter(val.get_type());
        param->set_value(val);
      }
    }
  }

  std::shared_ptr<bitfocus::module_handler> m_rc;
  ossia::net::network_context_ptr m_context;
  struct
  {
    ossia::net::node_base* actions{};
    ossia::net::node_base* presets{};
    ossia::net::node_base* feedbacks{};
    ossia::net::node_base* variables{};
  } nodes;

  ossia::flat_map<ossia::net::parameter_base*, QString> m_actions;
  ossia::flat_map<ossia::net::parameter_base*, QString> m_presets;
  ossia::flat_map<ossia::net::parameter_base*, QString> m_variables;
};
}
namespace Protocols
{

BitfocusDevice::BitfocusDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
  m_capas.canLearn = false;
  m_capas.hasCallbacks = false;
}

bool BitfocusDevice::reconnect()
{
  disconnect();

  try
  {
    const BitfocusSpecificSettings& stgs
        = settings().deviceSpecificSettings.value<BitfocusSpecificSettings>();
    if(!stgs.handler)
    {
      qDebug("oh noes");
      return false;
      ;
    }
    const auto& name = settings().name.toStdString();
    if(auto proto = std::make_unique<ossia::net::bitfocus_protocol>(stgs.handler, m_ctx))
    {
      m_dev = std::make_unique<ossia::net::generic_device>(std::move(proto), name);

      deviceChanged(nullptr, m_dev.get());
      setLogging_impl(Device::get_cur_logging(isLogging()));
    }
    else
    {
      qDebug() << "Could not create Bitfocus protocol";
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "Bitfocus Protocol error: " << e.what();
  }
  catch(...)
  {
    SCORE_TODO;
  }
  return connected();
}

bool BitfocusDevice::isLearning() const
{
  /*
  auto& proto = static_cast<ossia::net::bitfocus5_protocol&>(m_dev->get_protocol());
  return proto.learning();
  */
  return false;
}

void BitfocusDevice::setLearning(bool b)
{
  /*
  if(!m_dev)
    return;
  auto& proto = static_cast<ossia::net::bitfocus5_protocol&>(m_dev->get_protocol());
  auto& dev = *m_dev;
  if(b)
  {
    dev.on_node_created.connect<&DeviceInterface::nodeCreated>((DeviceInterface*)this);
    dev.on_node_removing.connect<&DeviceInterface::nodeRemoving>((DeviceInterface*)this);
    dev.on_node_renamed.connect<&DeviceInterface::nodeRenamed>((DeviceInterface*)this);
    dev.on_parameter_created.connect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.connect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }
  else
  {
    dev.on_node_created.disconnect<&DeviceInterface::nodeCreated>(
        (DeviceInterface*)this);
    dev.on_node_removing.disconnect<&DeviceInterface::nodeRemoving>(
        (DeviceInterface*)this);
    dev.on_node_renamed.disconnect<&DeviceInterface::nodeRenamed>(
        (DeviceInterface*)this);
    dev.on_parameter_created.disconnect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.disconnect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }

  proto.set_learning(b);*/
}
}
