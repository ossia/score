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
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>

#include <iscore/document/DocumentContext.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
LocalDevice::LocalDevice(
        const iscore::DocumentContext& ctx,
        std::shared_ptr<OSSIA::Device> dev,
        const iscore::DeviceSettings &settings):
    OSSIADevice{settings},
    m_context{ctx}
{
    m_dev = dev;
}

bool LocalDevice::reconnect()
{
    return connected();
}

bool LocalDevice::canRefresh() const
{
    return true;
}

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
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

    iscore_device.get<iscore::DeviceSettings>().name = settings().name;

    return iscore_device;
}
