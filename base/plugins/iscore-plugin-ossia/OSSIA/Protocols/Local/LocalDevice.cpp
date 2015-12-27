#include <API/Headers/Network/Device.h>
#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include "Network/Protocol/Local.h"
#include <OSSIA/Protocols/Local/LocalSpecificSettings.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

LocalDevice::LocalDevice(
        const iscore::DocumentContext& ctx,
        const std::shared_ptr<OSSIA::Device>& dev,
        const iscore::DeviceSettings &settings):
    OSSIADevice{settings}
{
    m_dev = dev;

    m_addedNodeCb = m_dev->addNodeCallbacks.addCallback(
                        [this] (const OSSIA::Node& n) {
        emit pathAdded(OSSIA::convert::ToAddress(n));
    });

    m_removedNodeCb = m_dev->removeNodeCallbacks.addCallback(
                          [this] (const OSSIA::Node& n) {
        emit pathRemoved(OSSIA::convert::ToAddress(n));
    });

    m_nameChangesCb = m_dev->nameChangesDeviceCallbacks.addCallback(
                          [this] (
                              const OSSIA::Node& node,
                              const std::string& oldName,
                              const std::string& newName) {
        iscore::Address currentAddress = OSSIA::convert::ToAddress(*node.getParent());
        currentAddress.path.push_back(QString::fromStdString(oldName));

        iscore::AddressSettings as = OSSIA::convert::ToAddressSettings(node);
        as.name = QString::fromStdString(newName);
        emit pathUpdated(currentAddress, as);
    });
}

LocalDevice::~LocalDevice()
{
    ISCORE_ASSERT(m_dev.get());
    m_dev->addNodeCallbacks.removeCallback(m_addedNodeCb);
    m_dev->removeNodeCallbacks.removeCallback(m_removedNodeCb);
    m_dev->nameChangesDeviceCallbacks.removeCallback(m_nameChangesCb);
}

void LocalDevice::disconnect()
{
    // TODO handle listening ??
}

bool LocalDevice::reconnect()
{
    return connected();
}

bool LocalDevice::canRefresh() const
{
    return true;
}

iscore::Node LocalDevice::refresh()
{
    iscore::Node iscore_device{settings(), nullptr};

    // Recurse on the children
    auto& ossia_children = m_dev->children();
    iscore_device.reserve(ossia_children.size());
    for(const auto& node : ossia_children)
    {
        iscore_device.push_back(OSSIA::convert::ToDeviceExplorer(*node.get()));
    }

    iscore_device.get<iscore::DeviceSettings>().name = QString::fromStdString(m_dev->getName());

    return iscore_device;
}
