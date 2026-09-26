// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/Bitfocus/BitfocusContext.hpp>
#include <Protocols/Bitfocus/BitfocusProtocol.hpp>
#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <memory>
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
  m_capas.canSerialize = true;
  m_capas.canLearn = false;
  m_capas.hasCallbacks = false;
}

bool BitfocusDevice::reconnect()
{
  disconnect();

  try
  {
    BitfocusSpecificSettings stgs
        = settings().deviceSpecificSettings.value<BitfocusSpecificSettings>();
    stgs.deduplicateConfiguration();

    const auto conf = stgs.moduleConfiguration();
    const auto label = settings().name;

    if(!stgs.handler)
    {
      stgs.handler = stgs.makeHandler(label);
      m_settings.deviceSpecificSettings = QVariant::fromValue(stgs);
    }

    stgs.handler->afterRegistration([label, conf, h = std::weak_ptr{stgs.handler}] {
      if(auto handler = h.lock())
        {
        // Keys the module saved itself stay, the document's values win
        auto merged = handler->model().config;
        for(auto& [k, v] : conf)
          merged[k] = v;
        handler->updateConfigAndLabel(label, merged);
      }
    });

    // What the module saves itself is kept with the document
    QObject::disconnect(m_configurationSaved);
    m_configurationSaved = connect(
        stgs.handler.get(), &bitfocus::module_handler::configurationSaved, this,
        [this, h = std::weak_ptr{stgs.handler}] {
      auto handler = h.lock();
      if(!handler)
        return;
      auto cur = m_settings.deviceSpecificSettings.value<BitfocusSpecificSettings>();
      if(cur.handler != handler)
        return;
      cur.configuration.clear();
      for(auto& [k, v] : handler->model().config)
        if(k != "product" || v != cur.product)
          cur.configuration.emplace_back(k, ossia::qt::qt_to_ossia{}(v));
      cur.upgradeIndex = handler->model().upgradeIndex;
      m_settings.deviceSpecificSettings = QVariant::fromValue(cur);
    });

    const auto& name = settings().name.toStdString();
    if(auto proto = std::make_unique<ossia::net::bitfocus_protocol>(stgs.handler, m_ctx))
    {
      auto pproto = proto.get();
      m_dev = std::make_shared<ossia::net::generic_device>(std::move(proto), name);

      deviceChanged(nullptr, m_dev.get());
      setLogging_impl(Device::get_cur_logging(isLogging()));

      stgs.handler->afterRegistration([dev = std::weak_ptr{m_dev}, pproto] {
        if(auto d = dev.lock())
          pproto->init_device();
      });
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
