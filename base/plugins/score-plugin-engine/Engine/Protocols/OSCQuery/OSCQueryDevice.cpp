// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <QVariant>
#include <memory>
#include <QTimer>

#include "OSCQueryDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/oscquery/oscquery_mirror.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/OSCQuery/OSCQuerySpecificSettings.hpp>

namespace Engine
{
namespace Network
{
OSCQueryDevice::OSCQueryDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;

  connect(this, &OSCQueryDevice::sig_command,
          this, &OSCQueryDevice::slot_command, Qt::QueuedConnection);
  connect(this, &OSCQueryDevice::sig_disconnect,
          this, &OSCQueryDevice::disconnect, Qt::QueuedConnection);
  reconnect();
}

bool OSCQueryDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs
        = settings().deviceSpecificSettings.value<OSCQuerySpecificSettings>();

    auto ossia_settings
        = std::make_unique<ossia::oscquery::oscquery_mirror_protocol>(
            stgs.host.toStdString());

    // run the commands in the Qt event loop
    // FIXME they should be disabled upon manual disconnection

    ossia_settings->set_command_callback([=] {
      sig_command();
    });
    ossia_settings->set_disconnect_callback([=] {
      sig_disconnect();
    });
    ossia_settings->set_fail_callback([=] {
      sig_disconnect();
    });

    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(ossia_settings), settings().name.toStdString());

    setLogging_impl(isLogging());

    enableCallbacks();
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

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
  if(m_dev)
  {
    auto proto = static_cast<ossia::oscquery::oscquery_mirror_protocol*>(&m_dev->get_protocol());
    proto->run_commands();
  }
}

}
}
