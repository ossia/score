#include <ossia/network/base/device.hpp>
#include <QString>
#include <QVariant>
#include <memory>
#include <iscore/document/DocumentContext.hpp>
#include <Engine/ApplicationPlugin.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include <ossia/network/local/local.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Engine/Protocols/Local/LocalSpecificSettings.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <ossia/network/minuit/minuit.hpp>
namespace Engine
{
namespace Network
{
LocalDevice::LocalDevice(
        ossia::net::generic_device& dev,
        const iscore::DocumentContext& ctx,
        const Device::DeviceSettings &settings):
    OSSIADevice{settings},
    m_dev{dev}
{
    m_capas.canRefreshTree = true;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;
    m_capas.canSerialize = false;

    auto& appplug = ctx.app.components.applicationPlugin<Engine::ApplicationPlugin>();

    auto& proto = safe_cast<ossia::net::local_protocol&>(dev.getProtocol());

    setLogging_impl(isLogging());

    auto& root = dev.getRootNode();

    {
        auto local_play_node = root.createChild("play");
        auto local_play_address = local_play_node->createAddress(ossia::val_type::BOOL);
        local_play_address->setValue(ossia::Bool{false});
        local_play_address->add_callback([&] (const ossia::value& v) {
            if (auto val = v.try_get<ossia::Bool>())
            {
                if(!appplug.playing() && val->value)
                {
                    // not playing, play requested
                    auto& play_action = appplug.context.actions.action<Actions::Play>();
                    play_action.action()->trigger();
                }
                else if(appplug.playing())
                {
                    if(appplug.paused() == val->value)
                    {
                        // paused, play requested
                        // or playing, pause requested

                        auto& play_action = appplug.context.actions.action<Actions::Play>();
                        play_action.action()->trigger();
                    }
                }
            }
        });
    }
    {
        auto local_stop_node = root.createChild("stop");
        auto local_stop_address = local_stop_node->createAddress(ossia::val_type::IMPULSE);
        local_stop_address->setValue(ossia::Impulse{});
        local_stop_address->add_callback([&] (const ossia::value&) {
            auto& stop_action = appplug.context.actions.action<Actions::Stop>();
            stop_action.action()->trigger();
        });
    }

    try {
    proto.exposeTo(std::make_unique<ossia::net::minuit_protocol>("i-score-remote", "127.0.0.1", 9999, 6666));
    }
    catch(...)
    {
        qDebug() << "LocalDevice: could not expose i-score-remote on port 6666";
    }

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

Device::Node LocalDevice::refresh()
{
    Device::Node iscore_device{settings(), nullptr};

    // Recurse on the children
    auto& ossia_children = m_dev.getRootNode().children();
    iscore_device.reserve(ossia_children.size());
    for(const auto& node : ossia_children)
    {
        iscore_device.push_back(Engine::ossia_to_iscore::ToDeviceExplorer(*node.get()));
    }

    iscore_device.get<Device::DeviceSettings>().name =
            QString::fromStdString(m_dev.getName());

    return iscore_device;
}
}
}
