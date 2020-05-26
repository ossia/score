#include "GfxDevice.hpp"
#include "GfxParameter.hpp"

#include <State/MessageListSerialization.hpp>
#include <ossia/network/base/device.hpp>

#include <QMimeData>


namespace Gfx
{

GfxInputDevice::GfxInputDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
  : Device::DeviceInterface{settings}, m_ctx{ctx}
{
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRefreshTree = false;
  m_capas.canRefreshValue = false;
  m_capas.hasCallbacks = false;
  m_capas.canListen = false;
  m_capas.canSerialize = true;
}

GfxInputDevice::~GfxInputDevice() { }

QMimeData* GfxInputDevice::mimeData() const
{
  auto mimeData = new QMimeData;

  State::Message mess;
  mess.address.address.device = m_settings.name;

  Mime<State::MessageList>::Serializer s{*mimeData};
  s.serialize({mess});
  return mimeData;
}

void GfxInputDevice::addAddress(const Device::FullAddressSettings& settings)
{
}

void GfxInputDevice::updateAddress(
    const State::Address& currentAddr,
    const Device::FullAddressSettings& settings)
{
}

void GfxInputDevice::disconnect()
{
}

void GfxInputDevice::recreate(const Device::Node& n)
{
}

void GfxInputDevice::setupNode(ossia::net::node_base& node, const ossia::extended_attributes& attr)
{
}

Device::Node GfxInputDevice::refresh()
{
  return simple_refresh();
}

GfxOutputDevice::GfxOutputDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
  : Device::DeviceInterface{settings}, m_ctx{ctx}
{
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRefreshTree = false;
  m_capas.canRefreshValue = false;
  m_capas.hasCallbacks = false;
  m_capas.canListen = false;
  m_capas.canSerialize = true;
}

GfxOutputDevice::~GfxOutputDevice() { }

QMimeData* GfxOutputDevice::mimeData() const
{
  auto mimeData = new QMimeData;

  State::Message mess;
  mess.address.address.device = m_settings.name;

  Mime<State::MessageList>::Serializer s{*mimeData};
  s.serialize({mess});
  return mimeData;
}

void GfxOutputDevice::addAddress(const Device::FullAddressSettings& settings)
{
}

void GfxOutputDevice::updateAddress(
    const State::Address& currentAddr,
    const Device::FullAddressSettings& settings)
{
}

void GfxOutputDevice::disconnect()
{
}

void GfxOutputDevice::recreate(const Device::Node& n)
{
}

void GfxOutputDevice::setupNode(ossia::net::node_base& node, const ossia::extended_attributes& attr)
{
}

Device::Node GfxOutputDevice::refresh()
{
  return simple_refresh();
}


}
