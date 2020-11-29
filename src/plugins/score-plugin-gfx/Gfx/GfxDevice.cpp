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
  m_capas.canRefreshTree = true;
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

static Device::Node ToDeviceExplorer(const ossia::net::node_base& ossia_node)
{
  Device::AddressSettings addr;
  addr.name = QString::fromStdString(ossia_node.get_name());

  Device::Node score_node{std::move(addr), nullptr};
  {
    const auto& cld = ossia_node.children();
    score_node.reserve(cld.size());

    // 2. Recurse on the children
    for (const auto& ossia_child : cld)
    {
      if (!ossia::net::get_hidden(*ossia_child) && !ossia::net::get_zombie(*ossia_child))
      {
        auto child_n = ToDeviceExplorer(*ossia_child);
        child_n.setParent(&score_node);
        score_node.push_back(std::move(child_n));
      }
    }
  }
  return score_node;
}

Device::Node GfxInputDevice::refresh()
{
  Device::Node score_device{settings(), nullptr};

  // Recurse on the children
  const auto& ossia_children = getDevice()->get_root_node().children();
  score_device.reserve(ossia_children.size());
  for (const auto& node : ossia_children)
  {
    score_device.push_back(ToDeviceExplorer(*node.get()));
  }

  score_device.get<Device::DeviceSettings>().name
      = QString::fromStdString(getDevice()->get_name());

  return score_device;
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
