#include <ossia/network/base/device.hpp>
#include <QString>
#include <QVariant>
#include <memory>

#include <Device/Protocol/DeviceSettings.hpp>
#include "LocalDevice.hpp"
#include <ossia/network/local/local.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <OSSIA/Protocols/Local/LocalSpecificSettings.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

namespace Ossia
{
namespace Protocols
{
LocalDevice::LocalDevice(
        const iscore::DocumentContext& ctx,
        const Device::DeviceSettings &settings):
    OSSIADevice{settings}
{
    m_dev = std::make_unique<impl::BasicDevice>(std::make_unique<impl::Local2>(), "i-score");
    m_capas.canRefreshTree = true;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;
    m_capas.canSerialize = false;


    setLogging_impl(isLogging());
    /*
     *
    auto& dev = *m_localDevice;
    auto& children = dev.children();
    {
        auto end = children.cend();
        auto local_play_node = *(m_localDevice->emplace(end, "play"));
        auto local_play_address = local_play_node->createAddress(OSSIA::Type::BOOL);
        local_play_address->setValue(OSSIA::Bool{false});
        local_play_address->addCallback([&] (const OSSIA::Value& v) {
            if (auto b = v.try_get<OSSIA::Bool>())
            {
                on_play(b->value);
            }
        });
    }
    {
        auto end = children.cend();
        auto local_stop_node = *(m_localDevice->emplace(end, "stop"));
        auto local_stop_address = local_stop_node->createAddress(OSSIA::Type::IMPULSE);
        local_stop_address->setValue(OSSIA::Impulse{});
        local_stop_address->addCallback([&] (const OSSIA::Value&) {
            on_stop();
        });

    }

    auto remote_protocol = OSSIA::Minuit::create("127.0.0.1", 9999, 6666);
    m_remoteDevice = OSSIA::Device::create(remote_protocol, "i-score-remote");

     */

    /*
    m_addedNodeCb = m_dev->addCallback(
                        [this] (const OSSIA::net::Node& n, const std::string& name, OSSIA::net::NodeChange chg)
    {
        if(chg == OSSIA::net::NodeChange::EMPLACED)
        {
            emit pathAdded(Ossia::convert::ToAddress(n));
        }
    });

    m_removedNodeCb = m_dev->addCallback(
                          [this] (const OSSIA::net::Node& n, const std::string& name, OSSIA::net::NodeChange chg)
    {
        if(chg == OSSIA::net::NodeChange::ERASED)
        {
            emit pathRemoved(Ossia::convert::ToAddress(n));
        }
    });

    m_nameChangesCb = m_dev->addCallback(
                          [this] (const OSSIA::net::Node& node, const std::string& old_name, OSSIA::net::NodeChange chg)
    {
        if(chg == OSSIA::net::NodeChange::RENAMED)
        {
            State::Address currentAddress = Ossia::convert::ToAddress(*node.getParent());
            currentAddress.path.push_back(QString::fromStdString(old_name));

            Device::AddressSettings as = Ossia::convert::ToAddressSettings(node);
            as.name = QString::fromStdString(node.getName());
            emit pathUpdated(currentAddress, as);
        }
    });
    */
}

LocalDevice::~LocalDevice()
{
    ISCORE_ASSERT(m_dev.get());
    /*
    m_dev->removeCallback(m_addedNodeCb);
    m_dev->removeCallback(m_removedNodeCb);
    m_dev->removeCallback(m_nameChangesCb);
    */
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
    auto& ossia_children = m_dev->getRootNode().children();
    iscore_device.reserve(ossia_children.size());
    for(const auto& node : ossia_children)
    {
        iscore_device.push_back(Ossia::convert::ToDeviceExplorer(*node.get()));
    }

    iscore_device.get<Device::DeviceSettings>().name = QString::fromStdString(m_dev->getName());

    return iscore_device;
}
}
}
