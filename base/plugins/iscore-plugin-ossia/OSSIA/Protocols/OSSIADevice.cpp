#include <QDebug>
#include <QStringList>
#include <algorithm>
#include <list>
#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include "OSSIADevice.hpp"
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/network_logger.hpp>

namespace Ossia
{
namespace Protocols
{
OSSIADevice::~OSSIADevice()
{
}

bool OSSIADevice::connected() const
{
    return bool(getDevice());
}

void OSSIADevice::updateSettings(const Device::DeviceSettings& newsettings)
{
    // TODO we have to maintain the prior connection state
    // if we were disconnected, we stay disconnected
    // else we reconnect. See in Minuit / MIDI also.
    if(auto dev = getDevice())
    {
        // First we save the existing nodes.
        Device::Node iscore_device{settings(), nullptr};

        // Recurse on the children
        auto& ossia_children = dev->getRootNode().children();
        iscore_device.reserve(ossia_children.size());
        for(const auto& node : ossia_children)
        {
            iscore_device.push_back(Ossia::convert::ToDeviceExplorer(*node.get()));
        }

        // We change the settings safely
        disconnect();

        m_settings = newsettings;

        if(reconnect())
        {
            // We can recreate our stuff.
            for(const auto& n : iscore_device.children())
            {
                addNode(n);
            }
        }
    }
    else
    {
        // We're already disconnected
        m_settings = newsettings;
    }
}

void OSSIADevice::disconnect()
{
    if(auto dev = getDevice())
    {
        auto& root = dev->getRootNode();
        removeListening_impl(root, State::Address{m_settings.name, {}});

        root.clearChildren();
    }

    m_callbacks.clear();
}

void OSSIADevice::addAddress(const Device::FullAddressSettings &settings)
{
    using namespace ossia;
    if(!m_capas.canAddNode)
        return; // TODO return bool instead, and check in the node update proxy ?

    if(auto dev = getDevice())
    {
        // Create the node. It is added into the device.
        ossia::net::Node* node = iscore::convert::createNodeFromPath(
                    settings.address.path,
                    *dev);
        ISCORE_ASSERT(node);

        // Populate the node with an address (if it isn't a no_value_t).
        iscore::convert::createOSSIAAddress(settings, *node);
    }
}


void OSSIADevice::updateAddress(
        const State::Address& currentAddr,
        const Device::FullAddressSettings &settings)
{
    if(auto dev = getDevice())
    {
        ossia::net::Node* node = iscore::convert::getNodeFromPath(
                    currentAddr.path,
                    *dev);
        auto newName = settings.address.path.last().toStdString();
        if(newName != node->getName())
        {
            node->setName(newName);
        }

        if(settings.value.val.which() == State::ValueType::NoValue)
        {
            node->removeAddress();
        }
        else
        {
            auto currentAddr = node->getAddress();
            if(currentAddr)
                iscore::convert::updateOSSIAAddress(settings, *currentAddr);
            else
                iscore::convert::createOSSIAAddress(settings, *node);
        }
    }

}

void OSSIADevice::removeListening_impl(
        ossia::net::Node& node, State::Address addr)
{
    // Find & remove our callback
    auto it = m_callbacks.find(addr);
    if(it != m_callbacks.end())
    {
        it->second.first->removeCallback(it->second.second);
        m_callbacks.erase(it);
    }

    // Recurse
    for(const auto& child : node.children())
    {
        State::Address sub_addr = addr;
        sub_addr.path += QString::fromStdString(child->getName());
        removeListening_impl(*child.get(), sub_addr);
    }
}

void OSSIADevice::setLogging_impl(bool b) const
{
    if(auto dev = getDevice())
    {
        if(b)
        {
            auto l = std::make_shared<ossia::NetworkLogger>();
            l->setInboundLogCallback([=] (std::string s) {
                emit logInbound(QString::fromStdString(s));
            });
            l->setOutboundLogCallback([=] (std::string s) {
                emit logOutbound(QString::fromStdString(s));
            });
            dev->getProtocol().setLogger(std::move(l));
            qDebug() << "logging enabled";
        }
        else
        {
            dev->getProtocol().setLogger({});
            qDebug() << "logging disabled";
        }
    }

}

void OSSIADevice::removeNode(const State::Address& address)
{
    using namespace ossia;
    if(!m_capas.canRemoveNode)
        return;
    if(auto dev = getDevice())
    {
        ossia::net::Node* node = iscore::convert::getNodeFromPath(address.path, *dev);
        auto parent = node->getParent();
        auto& parentChildren = node->getParent()->children();
        auto it = std::find_if(parentChildren.begin(), parentChildren.end(),
                               [&] (auto&& elt) { return elt.get() == node; });
        if(it != parentChildren.end())
        {
            /* If we are listening to this node, we recursively
             * remove listening to all the children. */
            removeListening_impl(*it->get(), address);

            // TODO !! if we remove nodes while recording
            // (or anything involving a registered listening state), there will be crashes.
            // The Device Explorer should be locked for edition during recording / playing.
            parent->removeChild((*it)->getName());
        }
    }
}

Device::Node OSSIADevice::refresh()
{
    Device::Node device_node{settings(), nullptr};

    if(auto dev = getDevice())
    {
        auto& root = dev->getRootNode();
        // Clear the listening
        removeListening_impl(root, State::Address{m_settings.name, {}});


        if(dev->getProtocol().update(root))
        {
            // Make a device explorer node from the current state of the device.
            // First make the node corresponding to the root node.

            // Recurse on the children
            auto& children = root.children();
            device_node.reserve(children.size());
            for(const auto& node : children)
            {
                device_node.push_back(Ossia::convert::ToDeviceExplorer(*node.get()));
            }
        }

        device_node.get<Device::DeviceSettings>().name = settings().name;
    }

    return device_node;
}

optional<State::Value> OSSIADevice::refresh(const State::Address& address)
{
    if(auto dev = getDevice())
    {
        auto node = iscore::convert::findNodeFromPath(address.path, *dev);
        if(node)
        {
            if(auto addr = node->getAddress())
            {
                addr->pullValue();
                return Ossia::convert::ToValue(addr->cloneValue());
            }
        }
    }

    return {};
}

Device::Node OSSIADevice::getNode(const State::Address& address)
{
    if(auto dev = getDevice())
    {
        auto ossia_node = iscore::convert::findNodeFromPath(address.path, *dev);
        if(ossia_node)
            return Ossia::convert::ToDeviceExplorer(*ossia_node);
    }

    return {};
}

void OSSIADevice::setListening(
        const State::Address& addr,
        bool b)
{
    if(auto dev = getDevice())
    {
        // First check if the address is already listening
        // so that we don't have to go through the tree.
        auto cb_it = m_callbacks.find(addr);

        ossia::net::address* ossia_addr{};
        if(cb_it == m_callbacks.end())
        {
            auto n = iscore::convert::findNodeFromPath(addr.path, *dev);
            if(!n)
                return;

            ossia_addr = n->getAddress();
            if(!ossia_addr)
                return;
        }
        else
        {
            ossia_addr = cb_it->second.first;
            if(!ossia_addr)
            {
                m_callbacks.erase(cb_it);
                return;
            }
        }

        ISCORE_ASSERT(bool(ossia_addr));

        // If we want to enable listening
        // and the address wasn't already listening
        if(b)
        {
            ossia_addr->pullValue();
            emit valueUpdated(addr, Ossia::convert::ToValue(ossia_addr->cloneValue()));

            if(cb_it == m_callbacks.end())
            {
                m_callbacks.insert(
                {
                    addr,
                    {
                         ossia_addr,
                         ossia_addr->addCallback([=] (const ossia::value& val)
                          {
                              emit valueUpdated(addr, Ossia::convert::ToValue(val));
                          })
                    }
                });
            }
        }
        else
        {
            // If we can disable listening
            if(cb_it != m_callbacks.end())
            {
                ossia_addr->removeCallback(cb_it->second.second);
                m_callbacks.erase(cb_it);
            }
        }
    }
}

std::vector<State::Address> OSSIADevice::listening() const
{
    if(!connected())
        return {};

    std::vector<State::Address> addrs;
    addrs.reserve(m_callbacks.size());

    for(const auto& elt : m_callbacks)
    {
        addrs.push_back(elt.first);
    }

    return addrs;
}

void OSSIADevice::addToListening(const std::vector<State::Address>& addresses)
{
    if(!connected())
        return;

    for(const auto& addr : addresses)
    {
        ISCORE_ASSERT(addr.device == this->settings().name);
        setListening(addr, true);
    }
}


void OSSIADevice::sendMessage(const State::Message& mess)
{
    if(auto dev = getDevice())
    {
        auto node = iscore::convert::getNodeFromPath(mess.address.path, *dev);

        auto addr = node->getAddress();
        if(addr)
        {
            addr->pushValue(iscore::convert::toOSSIAValue(mess.value));
        }
    }
}

bool OSSIADevice::isLogging() const
{
    return m_logging;
}

void OSSIADevice::setLogging(bool b)
{
    if(!connected())
        return;

    if(b == m_logging)
        return;

    m_logging = b;
    setLogging_impl(m_logging);
}

void OwningOSSIADevice::disconnect()
{
    OSSIADevice::disconnect();
    m_dev.reset();
}

}
}
