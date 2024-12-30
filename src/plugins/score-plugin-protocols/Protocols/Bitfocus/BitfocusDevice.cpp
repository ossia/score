// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>

#include <memory>
namespace Protocols
{

BitfocusDevice::BitfocusDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canLearn = true;
  m_capas.hasCallbacks = false;
}

bool BitfocusDevice::reconnect()
{
  disconnect();

  /*
  try
  {
    const BitfocusSpecificSettings& stgs
        = settings().deviceSpecificSettings.value<BitfocusSpecificSettings>();
    const auto& name = settings().name.toStdString();
    if(auto proto
       = std::make_unique<ossia::net::bitfocus5_protocol>(m_ctx, stgs.configuration))
    {
      if(stgs.rate)
      {
        auto rate = std::make_unique<ossia::net::rate_limiting_protocol>(
            std::chrono::milliseconds{*stgs.rate}, std::move(proto));
        m_dev = std::make_unique<ossia::net::generic_device>(std::move(rate), name);
      }
      else
      {
        m_dev = std::make_unique<ossia::net::generic_device>(std::move(proto), name);
      }

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
*/
  return connected();
}

void BitfocusDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
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
