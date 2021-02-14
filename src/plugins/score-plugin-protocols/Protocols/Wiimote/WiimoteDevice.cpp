#include "WiimoteDevice.hpp"
#include "WiimoteSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/protocols/wiimote/wiimote_protocol.hpp>

#include <QLabel>
#include <QProgressDialog>

#include <ossia-qt/invoke.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::WiimoteDevice)

namespace Protocols
{

WiimoteDevice::WiimoteDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
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
      auto& ctx = m_ctx.plugin<Explorer::DeviceDocumentPlugin>().asioContext;
      auto addr = std::make_unique<ossia::net::generic_device>(
          std::make_unique<ossia::net::wiimote_protocol>(ctx, false), settings().name.toStdString());

      m_dev = std::move(addr);
      deviceChanged(nullptr, m_dev.get());
    }
    catch (...)
    {
      SCORE_TODO;
    }
    ossia::qt::run_async(&dialog, [&dialog] { dialog.cancel(); });
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
