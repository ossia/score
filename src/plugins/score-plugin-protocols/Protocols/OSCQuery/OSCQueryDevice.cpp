// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQueryDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>

#include <Protocols/OSCQuery/OSCQuerySpecificSettings.hpp>

#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/oscquery/oscquery_mirror.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>
#include <ossia/network/resolve.hpp>
#include <ossia/protocols/oscquery/oscquery_mirror_asio.hpp>
#include <ossia/protocols/oscquery/oscquery_mirror_asio_dense.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QMetaMethod>

#include <wobjectimpl.h>

#include <memory>
W_OBJECT_IMPL(Protocols::OSCQueryDevice)
namespace Protocols
{

OSCQueryDevice::OSCQueryDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.asyncConnect = true;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;

  connect(
      this, &OSCQueryDevice::sig_command, this, &OSCQueryDevice::slot_command,
      Qt::QueuedConnection);
  connect(
      this, &OSCQueryDevice::sig_createDevice, this, &OSCQueryDevice::slot_createDevice,
      Qt::QueuedConnection);
  connect(
      this, &OSCQueryDevice::sig_disconnect, this,
      [this] {
    // TODO how to notify the GUI ?
    m_connected = false;
  },
      Qt::QueuedConnection);
}

OSCQueryDevice::~OSCQueryDevice()
{
  Device::releaseDevice(*m_ctx, std::move(m_dev));
}

bool OSCQueryDevice::connected() const
{
  return m_dev && m_connected;
}

void OSCQueryDevice::disconnect()
{
  if(m_mirror)
  {
    m_mirror->on_connection_closed.disconnect<&OSCQueryDevice::sig_disconnect>(*this);
    m_mirror->on_connection_failure.disconnect<&OSCQueryDevice::sig_disconnect>(*this);
    m_mirror = nullptr;
  }

  // Taken from OwningDeviceInterface::disconnect
  if(m_owned)
  {
    DeviceInterface::disconnect();
    // TODO why not auto dev = m_dev; ... like in MIDIDevice ?
    deviceChanged(m_dev.get(), nullptr);

    Device::releaseDevice(*m_ctx, std::move(m_dev));
    m_dev.reset();
  }
}

bool OSCQueryDevice::reconnect()
{
  const auto& cur_settings = settings();
  const auto& stgs
      = cur_settings.deviceSpecificSettings.value<OSCQuerySpecificSettings>();

  if(m_dev && m_mirror && m_oldSettings == cur_settings)
  {
    // TODO update settings
    try
    {
      m_mirror->connect();
      m_connected = true;
    }
    catch(std::exception& e)
    {
      qDebug() << "Could not connect: " << e.what();
      m_connected = false;
    }
    catch(...)
    {
      // TODO save the reason of the non-connection.
      m_connected = false;
    }

    connectionChanged(m_connected);
    return connected();
  }

  disconnect();

  // TODO put this in the io_context thread instead
  std::thread resolver([self = QPointer{this}, url = stgs.host.toStdString()] {
    auto [host, port] = ossia::url_to_host_and_port(url);
    bool ok = bool(ossia::resolve_sync_v4<boost::asio::ip::tcp>(host, port));
    QMetaObject::invokeMethod(qApp, [self, ok] {
      if(self)
      {
        if(ok)
        {
          self->sig_createDevice();
        }
        self->m_connected = false;
        self->connectionChanged(self->m_connected);
      }
    });
  });

  resolver.detach();

  return connected();
}

void OSCQueryDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

void OSCQueryDevice::slot_command()
{
  /*
  if (m_mirror)
  {
    m_mirror->run_commands();
  }
  */
}

void OSCQueryDevice::slot_createDevice()
{
  const auto& cur_settings = settings();
  const auto& stgs
      = cur_settings.deviceSpecificSettings.value<OSCQuerySpecificSettings>();

  try
  {
    std::unique_ptr<ossia::net::protocol_base> ossia_settings;
    if(stgs.dense)
    {
      ossia_settings
          = std::make_unique<ossia::oscquery_asio::oscquery_mirror_asio_protocol_dense>(
              m_ctx, stgs.host.toStdString(), stgs.localPort);
    }
    else
    {
      ossia_settings
          = std::make_unique<ossia::oscquery_asio::oscquery_mirror_asio_protocol>(
              m_ctx, stgs.host.toStdString(), stgs.localPort);
    }

    m_mirror = ossia_settings.get();

    if(stgs.rate)
    {
      ossia_settings = std::make_unique<ossia::net::rate_limiting_protocol>(
          std::chrono::milliseconds{*stgs.rate}, std::move(ossia_settings));
    }

    // run the commands in the Qt event loop
    // FIXME they should be disabled upon manual disconnection

    Device::releaseDevice(*m_ctx, std::move(m_dev));
    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(ossia_settings), settings().name.toStdString());

    deviceChanged(nullptr, m_dev.get());

    //m_mirror->set_command_callback([this] { sig_command(); });
    m_mirror->on_connection_closed.connect<&OSCQueryDevice::sig_disconnect>(*this);
    m_mirror->on_connection_failure.connect<&OSCQueryDevice::sig_disconnect>(*this);

    setLogging_impl(Device::get_cur_logging(isLogging()));

    enableCallbacks();
    m_connected = true;
  }
  catch(std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
    m_connected = false;
    if(!m_dev)
      m_mirror = nullptr;
  }
  catch(...)
  {
    // TODO save the reason of the non-connection.
    m_connected = false;
    if(!m_dev)
      m_mirror = nullptr;
  }

  connectionChanged(m_connected);
}
}
