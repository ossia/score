#include <Network/Device.h>
#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include "Network/Protocol/Local.h"
#include <OSSIA/Protocols/Local/LocalSpecificSettings.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

namespace Ossia
{
LocalDevice::LocalDevice(
        const iscore::DocumentContext& ctx,
        const std::shared_ptr<OSSIA::Device>& dev,
        const Device::DeviceSettings &settings):
    OSSIADevice{settings}
{
    m_dev = dev;
    m_capas.canRefreshTree = true;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;

    m_addedNodeCb = m_dev->addCallback(
                        [this] (const OSSIA::Node& n, const std::string& name, OSSIA::NodeChange chg)
    {
        if(chg == OSSIA::NodeChange::EMPLACED)
        {
            emit pathAdded(Ossia::convert::ToAddress(n));
        }
    });

    m_removedNodeCb = m_dev->addCallback(
                          [this] (const OSSIA::Node& n, const std::string& name, OSSIA::NodeChange chg)
    {
        if(chg == OSSIA::NodeChange::ERASED)
        {
            emit pathRemoved(Ossia::convert::ToAddress(n));
        }
    });

    m_nameChangesCb = m_dev->addCallback(
                          [this] (const OSSIA::Node& node, const std::string& old_name, OSSIA::NodeChange chg)
    {
        if(chg == OSSIA::NodeChange::RENAMED)
        {
            State::Address currentAddress = Ossia::convert::ToAddress(*node.getParent());
            currentAddress.path.push_back(QString::fromStdString(old_name));

            Device::AddressSettings as = Ossia::convert::ToAddressSettings(node);
            as.name = QString::fromStdString(node.getName());
            emit pathUpdated(currentAddress, as);
        }
    });
}

LocalDevice::~LocalDevice()
{
    ISCORE_ASSERT(m_dev.get());
    m_dev->removeCallback(m_addedNodeCb);
    m_dev->removeCallback(m_removedNodeCb);
    m_dev->removeCallback(m_nameChangesCb);
}

void LocalDevice::disconnect()
{
    // TODO handle listening ??
}

bool LocalDevice::reconnect()
{
    m_callbacks.clear();
    return connected();
}

Device::Node LocalDevice::refresh()
{
    Device::Node iscore_device{settings(), nullptr};

    // Recurse on the children
    auto& ossia_children = m_dev->children();
    iscore_device.reserve(ossia_children.size());
    for(const auto& node : ossia_children)
    {
        iscore_device.push_back(Ossia::convert::ToDeviceExplorer(*node.get()));
    }

    iscore_device.get<Device::DeviceSettings>().name = QString::fromStdString(m_dev->getName());

    return iscore_device;
}
}
