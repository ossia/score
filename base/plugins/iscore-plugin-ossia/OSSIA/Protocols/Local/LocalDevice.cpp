#include <ossia/network/base/device.hpp>
#include <QString>
#include <QVariant>
#include <memory>
#include <iscore/document/DocumentContext.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include <ossia/network/local/local.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <OSSIA/Protocols/Local/LocalSpecificSettings.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <ossia/network/minuit/minuit.hpp>
namespace Ossia
{
namespace Protocols
{
LocalDevice::LocalDevice(
        const iscore::DocumentContext& ctx,
        const Device::DeviceSettings &settings):
    OSSIADevice{settings}
{
    m_capas.canRefreshTree = true;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;
    m_capas.canSerialize = false;

    auto& appplug = ctx.app.components.applicationPlugin<OSSIAApplicationPlugin>();
    auto docplug = ctx.findPlugin<Ossia::LocalTree::DocumentPlugin>();
    if(!docplug)
    {
        // There is no local device in this document
        return;
    }

    auto& dev = docplug->device();
    m_dev = &dev;
    auto& proto = safe_cast<ossia::net::local_protocol&>(dev.getProtocol());

    setLogging_impl(isLogging());

    auto& root = dev.getRootNode();

    {
        auto local_play_node = root.createChild("play");
        auto local_play_address = local_play_node->createAddress(ossia::val_type::BOOL);
        local_play_address->setValue(ossia::Bool{false});
        local_play_address->addCallback([&] (const ossia::value& v) {
            if (v.try_get<ossia::Bool>())
            {
                auto& play_action = appplug.context.actions.action<Actions::Play>();
                play_action.action()->trigger();
            }
        });
    }
    {
        auto local_stop_node = root.createChild("stop");
        auto local_stop_address = local_stop_node->createAddress(ossia::val_type::IMPULSE);
        local_stop_address->setValue(ossia::Impulse{});
        local_stop_address->addCallback([&] (const ossia::value&) {
            auto& stop_action = appplug.context.actions.action<Actions::Stop>();
            stop_action.action()->trigger();
        });
    }

    proto.exposeTo(std::make_unique<ossia::net::minuit_protocol>("127.0.0.1", 9999, 6666));

    dev.onNodeCreated.connect<LocalDevice, &LocalDevice::nodeCreated>(this);
    dev.onNodeRemoving.connect<LocalDevice, &LocalDevice::nodeRemoving>(this);
    dev.onNodeRenamed.connect<LocalDevice, &LocalDevice::nodeRenamed>(this);
}

LocalDevice::~LocalDevice()
{
}

void LocalDevice::disconnect()
{
    // TODO handle listening ??
    setLogging_impl(false);
}

bool LocalDevice::reconnect()
{
    m_callbacks.clear();
    return connected();
}

void LocalDevice::nodeCreated(const ossia::net::node_base & n)
{
    emit pathAdded(Ossia::convert::ToAddress(n));
}

void LocalDevice::nodeRemoving(const ossia::net::node_base & n)
{
    emit pathRemoved(Ossia::convert::ToAddress(n));
}

void LocalDevice::nodeRenamed(const ossia::net::node_base& node, std::string old_name)
{
    if(!node.getParent())
        return;

    State::Address currentAddress = Ossia::convert::ToAddress(*node.getParent());
    currentAddress.path.push_back(QString::fromStdString(old_name));

    Device::AddressSettings as = Ossia::convert::ToAddressSettings(node);
    as.name = QString::fromStdString(node.getName());
    emit pathUpdated(currentAddress, as);

}

Device::Node LocalDevice::refresh()
{
    if(m_dev)
    {
        Device::Node iscore_device{settings(), nullptr};

        // Recurse on the children
        auto& ossia_children = m_dev->getRootNode().children();
        iscore_device.reserve(ossia_children.size());
        for(const auto& node : ossia_children)
        {
            iscore_device.push_back(Ossia::convert::ToDeviceExplorer(*node.get()));
        }

        iscore_device.get<Device::DeviceSettings>().name = QString::fromStdString(m_dev->getName());

        return iscore_device;
    }
    return {};
}
}
}
