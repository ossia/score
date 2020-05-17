
#include "WiimoteDevice.hpp"

#include "WiimoteSpecificSettings.hpp"

#include <ossia/network/wiimote/wiimote_protocol.hpp>

#include <QLabel>
#include <QProgressDialog>

#include <thread>

W_OBJECT_IMPL(Protocols::WiimoteDevice)

namespace Protocols
{

WiimoteDevice::WiimoteDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

WiimoteDevice::~WiimoteDevice() { }

bool WiimoteDevice::reconnect()
{
  disconnect();

  QProgressDialog dialog;

  dialog.setRange(0, 0);
  dialog.setLabel(new QLabel(tr("Looking for wiimotes")));
  dialog.setCancelButton(nullptr);
  dialog.setWindowFlags(Qt::FramelessWindowHint);

  std::thread task{[&dialog, this]() {
    try
    {
      auto addr = std::make_unique<ossia::net::generic_device>(
          std::make_unique<ossia::net::wiimote_protocol>(false), settings().name.toStdString());

      m_dev = std::move(addr);
      deviceChanged(nullptr, m_dev.get());
    }
    catch (...)
    {
      SCORE_TODO;
    }
    dialog.cancel();
  }};

  dialog.exec();
  task.join();

  return connected();
}

void WiimoteDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}

}
