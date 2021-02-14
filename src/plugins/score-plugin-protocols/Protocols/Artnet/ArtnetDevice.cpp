#include <ossia/detail/config.hpp>

#include <QDebug>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetDevice.hpp"
#include "ArtnetSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/protocols/artnet/artnet_protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::ArtnetDevice)

namespace Protocols
{

ArtnetDevice::ArtnetDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
  : OwningDeviceInterface{settings}
  , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

ArtnetDevice::~ArtnetDevice() { }

bool ArtnetDevice::reconnect()
{
  disconnect();

  try
  {
    //  20 Hz update rate : TODO add control on widget
    auto& ctx = m_ctx.plugin<Explorer::DeviceDocumentPlugin>().asioContext;
    auto addr = std::make_unique<ossia::net::generic_device>(
        std::make_unique<ossia::net::artnet_protocol>(ctx, 20),
        settings().name.toStdString());
    m_dev = std::move(addr);
    deviceChanged(nullptr, m_dev.get());
  }
  catch (const std::runtime_error& e)
  {
    qDebug() << "ArtNet error: " << e.what();
  }
  catch (...)
  {
    qDebug() << "ArtNet error";
  }

  return connected();
}

void ArtnetDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}
#endif
