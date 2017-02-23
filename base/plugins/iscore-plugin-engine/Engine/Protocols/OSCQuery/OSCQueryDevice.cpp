#include <QString>
#include <QVariant>
#include <memory>
#include <QTimer>

#include "OSCQueryDevice.hpp"
#include <ossia/network/generic/generic_address.hpp>
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
    ossia_settings->setCommandCallback([=] { sig_command(); });
    ossia_settings->setDisconnectCallback([=] {
      emit sig_disconnect();
    });
    ossia_settings->setFailCallback([=] {
      emit sig_disconnect();
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
    auto proto = safe_cast<ossia::oscquery::oscquery_mirror_protocol*>(&m_dev->getProtocol());
    proto->runCommands();
  }
}

}
}
