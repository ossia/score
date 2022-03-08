// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQueryDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Protocols/OSCQuery/OSCQuerySpecificSettings.hpp>
#include <ossia/protocols/oscquery/oscquery_mirror_asio.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/oscquery/oscquery_mirror.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>
#include <ossia/network/context.hpp>
#include <boost/algorithm/string.hpp>

#include <QDebug>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <wobjectimpl.h>

#include <memory>
W_OBJECT_IMPL(Protocols::OSCQueryDevice)

bool resolve_ip(std::string host)
{
  try
  {
    std::string m_queryPort;
    auto m_queryHost = host;
    auto port_idx = m_queryHost.find_last_of(':');
    if (port_idx != std::string::npos)
    {
      m_queryPort = m_queryHost.substr(port_idx + 1);
      m_queryHost = m_queryHost.substr(0, port_idx);
    }
    else
      m_queryPort = "80";

    if (boost::starts_with(m_queryHost, "http://"))
      m_queryHost.erase(m_queryHost.begin(), m_queryHost.begin() + 7);
    else if (boost::starts_with(m_queryHost, "ws://"))
      m_queryHost.erase(m_queryHost.begin(), m_queryHost.begin() + 5);

    boost::asio::io_service io_service;
    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(m_queryHost, m_queryPort);
    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
    return true;
  }
  catch (const std::exception& e)
  {
  }
  catch (...)
  {
  }
  return false;
}
namespace Protocols
{
OSCQueryDevice::OSCQueryDevice(const Device::DeviceSettings& settings,
                               const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.asyncConnect = true;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;

  connect(
      this,
      &OSCQueryDevice::sig_command,
      this,
      &OSCQueryDevice::slot_command,
      Qt::QueuedConnection);
  connect(
      this,
      &OSCQueryDevice::sig_createDevice,
      this,
      &OSCQueryDevice::slot_createDevice,
      Qt::QueuedConnection);
  connect(
      this,
      &OSCQueryDevice::sig_disconnect,
      this,
      [=] {
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
  if (m_mirror)
  {
    m_mirror->on_connection_closed.disconnect<&OSCQueryDevice::sig_disconnect>(*this);
    m_mirror->on_connection_failure.disconnect<&OSCQueryDevice::sig_disconnect>(*this);
    m_mirror = nullptr;
  }

  // Taken from OwningDeviceInterface::disconnect
  if (m_owned)
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

  if (m_dev && m_mirror && m_oldSettings == cur_settings)
  {
    // TODO update settings
    try
    {
      m_mirror->connect();
      m_connected = true;
    }
    catch (std::exception& e)
    {
      qDebug() << "Could not connect: " << e.what();
      m_connected = false;
    }
    catch (...)
    {
      // TODO save the reason of the non-connection.
      m_connected = false;
    }

    connectionChanged(m_connected);
    return connected();
  }

  disconnect();

  // TODO put this in the io_context thread instead
  std::thread resolver([this, host = stgs.host.toStdString()] {
    bool ok = resolve_ip(host);
    if (ok)
    {
      sig_createDevice();
    }

    // FIXME the device could be deleted there !
    m_connected = false;
    connectionChanged(m_connected);
  });

  resolver.detach();

  return connected();
}

void OSCQueryDevice::recreate(const Device::Node& n)
{
  for (auto& child : n)
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
    std::unique_ptr<ossia::net::protocol_base> ossia_settings
        = std::make_unique<mirror_proto>(m_ctx, stgs.host.toStdString());

    auto& p = static_cast<mirror_proto&>(
        *ossia_settings);
    m_mirror = &p;

    if (stgs.rate)
    {
      ossia_settings = std::make_unique<ossia::net::rate_limiting_protocol>(
          std::chrono::milliseconds{*stgs.rate}, std::move(ossia_settings));
    }

    // run the commands in the Qt event loop
    // FIXME they should be disabled upon manual disconnection

    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(ossia_settings), settings().name.toStdString());

    deviceChanged(nullptr, m_dev.get());

    //p.set_command_callback([=] { sig_command(); });
    p.on_connection_closed.connect<&OSCQueryDevice::sig_disconnect>(*this);
    p.on_connection_failure.connect<&OSCQueryDevice::sig_disconnect>(*this);

    setLogging_impl(Device::get_cur_logging(isLogging()));

    enableCallbacks();
    m_connected = true;
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
    m_connected = false;
    if (!m_dev)
      m_mirror = nullptr;
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
    m_connected = false;
    if (!m_dev)
      m_mirror = nullptr;
  }

  connectionChanged(m_connected);
}
}
