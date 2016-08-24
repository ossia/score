#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_address.hpp>
#include <ossia/network/http/http.hpp>
#include "HTTPDevice.hpp"
#include <Engine/Protocols/HTTP/HTTPSpecificSettings.hpp>

namespace Engine
{
namespace Network
{
HTTPDevice::HTTPDevice(const Device::DeviceSettings &settings):
    OwningOSSIADevice{settings}
{
    m_capas.canRefreshTree = true;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;
    m_capas.canSerialize = false;
    reconnect();
}

bool HTTPDevice::reconnect()
{
    disconnect();

    try {
        auto stgs = settings().deviceSpecificSettings.value<HTTPSpecificSettings>();

        m_dev = std::make_unique<ossia::net::http_device>(
              std::make_unique<ossia::net::http_protocol>(stgs.text.toUtf8()),
              settings().name.toStdString());

        m_dev->onNodeCreated.connect<HTTPDevice, &HTTPDevice::nodeCreated>(this);
        m_dev->onNodeRemoving.connect<HTTPDevice, &HTTPDevice::nodeRemoving>(this);
        m_dev->onNodeRenamed.connect<HTTPDevice, &HTTPDevice::nodeRenamed>(this);

        setLogging_impl(isLogging());
    }
    catch(std::exception& e)
    {
        qDebug() << "Could not connect: " << e.what();
    }
    catch(...)
    {
        // TODO save the reason of the non-connection.
    }

    return connected();
}
}
}
