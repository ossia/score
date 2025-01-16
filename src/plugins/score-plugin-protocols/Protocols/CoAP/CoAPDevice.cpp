// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CoAPDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/CoAP/CoAPSpecificSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>

#include <memory>
namespace Protocols
{

CoAPDevice::CoAPDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.asyncConnect = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
}

bool CoAPDevice::reconnect()
{
  disconnect();

  try
  {
    const CoAPSpecificSettings& stgs
        = settings().deviceSpecificSettings.value<CoAPSpecificSettings>();
    const auto& name = settings().name.toStdString();
    if(auto proto
       = std::make_unique<ossia::net::coap_client_protocol>(m_ctx, stgs.configuration))
    {
      if(stgs.rate)
      {
        ossia::net::rate_limiter_configuration conf{
            .duration = std::chrono::milliseconds(*stgs.rate)};
        auto rate = std::make_unique<ossia::net::rate_limiting_protocol>(
            conf, std::move(proto));
        m_dev = std::make_unique<ossia::net::generic_device>(std::move(rate), name);
      }
      else
      {
        m_dev = std::make_unique<ossia::net::generic_device>(std::move(proto), name);
      }

      deviceChanged(nullptr, m_dev.get());
      setLogging_impl(Device::get_cur_logging(isLogging()));
    }
    else
    {
      qDebug() << "Could not create CoAP protocol";
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "CoAP Protocol error: " << e.what();
  }
  catch(...)
  {
    SCORE_TODO;
  }

  return connected();
}

void CoAPDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

bool CoAPDevice::isLearning() const
{
  return false;
}

void CoAPDevice::setLearning(bool b) { }
}
