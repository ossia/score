// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <QVariant>
#include <memory>

#include "PhidgetsDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/phidgets/phidgets_protocol.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/Phidgets/PhidgetsSpecificSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <QTimer>

namespace Engine
{
namespace Network
{
PhidgetDevice::PhidgetDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  connect(this, &PhidgetDevice::sig_command,
          this, &PhidgetDevice::slot_command, Qt::QueuedConnection);

  reconnect();
}

bool PhidgetDevice::reconnect()
{
  if(m_timer != -1)
  {
    killTimer(m_timer);
    m_timer = -1;
  }
  disconnect();

  try
  {
    //const auto& stgs = settings().deviceSpecificSettings.value<PhidgetSpecificSettings>();

    m_dev = std::make_unique<ossia::net::generic_device>(std::make_unique<ossia::phidget_protocol>(), settings().name.toStdString());
    enableCallbacks();
    m_timer = startTimer(200);
    /*
    QTimer::singleShot(5000, [=] {

      if(m_dev)
      {
        auto proto = dynamic_cast<ossia::phidget_protocol*>(&m_dev->get_protocol());
        proto->run_commands();

        static_cast<ossia::phidget_protocol&>(m_dev->get_protocol())
            .set_command_callback([=] { sig_command(); });
      }

    });
    */


    setLogging_impl(Device::get_cur_logging(isLogging()));
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

void PhidgetDevice::slot_command()
{
  if(m_dev)
  {
    auto proto = dynamic_cast<ossia::phidget_protocol*>(&m_dev->get_protocol());
    proto->run_commands();
  }
}

void PhidgetDevice::timerEvent(QTimerEvent* event)
{
  if(m_dev)
  {
    auto proto = dynamic_cast<ossia::phidget_protocol*>(&m_dev->get_protocol());
    proto->run_command();
  }
}

}
}

