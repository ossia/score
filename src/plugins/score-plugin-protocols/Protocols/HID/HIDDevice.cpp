#include "HIDDevice.hpp"

#include "HIDSpecificSettings.hpp"

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/hid/hid_protocol.hpp>

#include <QDebug>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::HIDDevice)

namespace Protocols
{
static ossia::net::hid_device_configuration toConfiguration(const HIDSpecificSettings& s)
{
  return {
      .path = s.path.toStdString(),
      .serial = s.serial.toStdString(),
      .vendor_id = uint16_t(s.vendorId),
      .product_id = uint16_t(s.productId),
      .interface_number = s.interfaceNumber};
}

HIDDevice::HIDDevice(
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

  connect(this, &HIDDevice::unplugged, this, &HIDDevice::on_unplugged, Qt::QueuedConnection);

  m_replug.setInterval(1000);
  connect(&m_replug, &QTimer::timeout, this, [this] {
    auto set = this->settings().deviceSpecificSettings.value<HIDSpecificSettings>();
    if(ossia::net::hid_find_device(toConfiguration(set)))
    {
      m_replug.stop();
      if(reconnect())
        namespaceUpdated();
    }
  });
}

HIDDevice::~HIDDevice() { }

bool HIDDevice::reconnect()
{
  disconnect();

  auto set = settings().deviceSpecificSettings.value<HIDSpecificSettings>();
  try
  {
    auto proto = std::make_unique<ossia::net::hid_protocol>(m_ctx, toConfiguration(set));
    proto->on_connection_closed.connect<&HIDDevice::unplugged>(*this);
    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(proto), settings().name.toStdString());
    deviceChanged(nullptr, m_dev.get());
  }
  catch(const std::exception& e)
  {
    qDebug() << "HID:" << e.what();
    m_replug.start();
  }

  return connected();
}

void HIDDevice::disconnect()
{
  if(m_dev)
  {
    auto& proto = static_cast<ossia::net::hid_protocol&>(m_dev->get_protocol());
    proto.stop();
    proto.on_connection_closed.disconnect<&HIDDevice::unplugged>(*this);
  }
  OwningDeviceInterface::disconnect();
}

void HIDDevice::on_unplugged()
{
  if(m_dev && m_dev->get_protocol().connected())
    return;
  disconnect();
  m_replug.start();
}
}
