#include "MinuitDevice.hpp"
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Editor/Value.h>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"
using namespace iscore::convert;
using namespace OSSIA::convert;

MinuitDevice::MinuitDevice(const iscore::DeviceSettings &settings):
    OSSIADevice{settings},
    m_minuitSettings{[&] () {
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
    return OSSIA::Minuit::create(stgs.host.toStdString(),
        stgs.inPort,
        stgs.outPort);
    }()
    }
{
    using namespace OSSIA;

    m_dev = Device::create(m_minuitSettings, settings.name.toStdString());
}

void MinuitDevice::updateSettings(const iscore::DeviceSettings& settings)
{
    m_settings = settings;
    m_dev->setName(m_settings.name.toStdString());
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();

    // TODO m_dev->setName(m_settings.name.toStdString());

    auto prot = dynamic_cast<OSSIA::Minuit*>(m_dev->getProtocol().get());
    prot->setInPort(stgs.inPort);
    prot->setOutPort(stgs.outPort);
    prot->setIp(stgs.host.toStdString());
}

bool MinuitDevice::canRefresh() const
{
    return true;
}

#include <QDebug>
iscore::Node MinuitDevice::MinuitToDeviceExplorer(
        const OSSIA::Node &node,
        iscore::Address currentAddr)
{
    iscore::Node n{ToAddressSettings(node), nullptr};

    currentAddr.path += n.get<iscore::AddressSettings>().name;

    // 2. Add a callback
    if(n.get<iscore::AddressSettings>().ioType != iscore::IOType::Invalid)
    {
        if(auto ossia_addr = node.getAddress())
        {
            ossia_addr->addCallback([=] (const OSSIA::Value* val) {
                emit valueUpdated(currentAddr, OSSIA::convert::ToValue(val));
            });
        }
    }

    // 3. Recurse on the children
    for(const auto& ossia_child : node.children())
    {
        auto child_n = MinuitToDeviceExplorer(*ossia_child.get(), currentAddr);
        child_n.setParent(&n);
        n.push_back(std::move(child_n));
    }

    return n;
}


iscore::Node MinuitDevice::refresh()
{
    iscore::Node device_node;

    if(m_dev->updateNamespace())
    {
        // Make a device explorer node from the current state of the device.
        // First make the node corresponding to the root node.

        device_node.set(settings());

        iscore::Address addr;
        addr.device = settings().name;

        // Recurse on the children
        for(const auto& node : m_dev->children())
        {
            device_node.push_back(MinuitToDeviceExplorer(*node.get(), addr));
        }
    }

    device_node.get<iscore::DeviceSettings>().name = settings().name;

    return device_node;
}

iscore::Value MinuitDevice::refresh(const iscore::Address & address)
{
    OSSIA::Node* node = getNodeFromPath(address.path, m_dev.get());
    auto addr = node->getAddress();
    addr->pullValue();
    return ToValue(addr->getValue());
}
