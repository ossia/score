#include "LeapmotionDevice.hpp"

#include "LeapmotionSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/leapmotion/leapmotion_device.hpp>

#include <ossia-qt/invoke.hpp>

#include <QLabel>
#include <QProgressDialog>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::LeapmotionDevice)

namespace Protocols
{

LeapmotionDevice::LeapmotionDevice(
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

LeapmotionDevice::~LeapmotionDevice() { }

bool LeapmotionDevice::reconnect()
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
          std::make_unique<ossia::leapmotion_protocol>(m_ctx),
          settings().name.toStdString());

      m_dev = std::move(addr);
      deviceChanged(nullptr, m_dev.get());
    }
    catch(...)
    {
      SCORE_TODO;
    }
    ossia::qt::run_async(&dialog, [&dialog] { dialog.cancel(); });
  }};

  dialog.exec();
  task.join();

  return connected();
}

void LeapmotionDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}

}
