#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetDevice.hpp"
#include "ArtnetSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/Artnet/DMXFixtureInstantiation.hpp>
#include <Protocols/Artnet/DMXProtocolCreation.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::ArtnetDevice)

namespace Protocols
{

ArtnetDevice::ArtnetDevice(
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
}

ArtnetDevice::~ArtnetDevice() { }

bool ArtnetDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set = m_settings.deviceSpecificSettings.value<ArtnetSpecificSettings>();

    // Create the device
    if(auto artnet_proto = Protocols::instantiateDMXProtocol(m_ctx, set))
    {
      auto& proto = *artnet_proto;
      SCORE_ASSERT(proto.buffer().universes() >= 1);

      auto dev = std::make_unique<ossia::net::generic_device>(
          std::move(artnet_proto), settings().name.toStdString());

      for(auto& fixt : set.fixtures)
      {
        addArtnetFixture(*dev, proto.buffer(), fixt, set.channels_per_universe);
      }

      if(set.mode == ArtnetSpecificSettings::Sink)
        if(auto p = dynamic_cast<ossia::net::dmx_input_protocol_base*>(&proto))
          p->create_channel_map();
      m_dev = std::move(dev);
      deviceChanged(nullptr, m_dev.get());
    }
  }
  catch(const std::runtime_error& e)
  {
    qDebug() << "ArtNet error: " << e.what();
  }
  catch(...)
  {
    qDebug() << "ArtNet error";
  }

  return connected();
}

void ArtnetDevice::disconnect()
{
  if(m_owned)
  {
    if(m_capas.hasCallbacks)
      disableCallbacks();

    m_callbacks.clear();
    deviceChanged(m_dev.get(), nullptr);
    Device::releaseDevice(*this->m_ctx, std::move(m_dev));
  }
}
}
#endif
