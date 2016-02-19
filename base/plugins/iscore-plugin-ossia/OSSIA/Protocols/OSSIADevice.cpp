#include <Network/Node.h>
#include <QDebug>
#include <QStringList>
#include <algorithm>
#include <list>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include "Network/Address.h"
#include "Network/Device.h"
#include "OSSIADevice.hpp"
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

namespace OSSIA {
class Value;
}  // namespace OSSIA

using namespace iscore::convert;
using namespace Ossia::convert;

namespace Ossia
{
OSSIADevice::~OSSIADevice()
{
    if(connected())
        disconnect();
}

bool OSSIADevice::connected() const
{
    return bool(m_dev);
}

void OSSIADevice::updateSettings(const Device::DeviceSettings& newsettings)
{
    // TODO we have to maintain the prior connection state
    // if we were disconnected, we stay disconnected
    // else we reconnect. See in Minuit / MIDI also.
    if(connected())
    {
        // First we save the existing nodes.
        Device::Node iscore_device{settings(), nullptr};

        // Recurse on the children
        auto& ossia_children = m_dev->children();
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
    if(connected())
    {
        removeListening_impl(*m_dev.get(), State::Address{m_settings.name, {}});
    }

    m_callbacks.clear();


    auto& children = m_dev->children();
    while(!children.empty())
        m_dev->erase(children.end() - 1);
    m_dev.reset();
}

void OSSIADevice::addAddress(const Device::FullAddressSettings &settings)
{
    using namespace OSSIA;
    if(!m_capas.canAddNode)
        return; // TODO return bool instead, and check in the node update proxy ?

    if(!connected())
        return;

    // Create the node. It is added into the device.
    OSSIA::Node* node = createNodeFromPath(settings.address.path, m_dev.get());

    // Populate the node with an address (if it isn't a no_value_t).
    createOSSIAAddress(settings, node);
}


void OSSIADevice::updateAddress(
        const State::Address& currentAddr,
        const Device::FullAddressSettings &settings)
{
    if(!connected())
        return;

    OSSIA::Node* node = getNodeFromPath(currentAddr.path, m_dev.get());
    auto newName = settings.address.path.last().toStdString();
    if(newName != node->getName())
    {
        node->setName(newName);
    }

    if(settings.value.val.which() == State::ValueType::NoValue)
    {
        removeOSSIAAddress(node);
    }
    else
    {
        auto currentAddr = node->getAddress();
        if(currentAddr)
            updateOSSIAAddress(settings, node->getAddress());
        else
            createOSSIAAddress(settings, node);
    }
}

void OSSIADevice::removeListening_impl(
        OSSIA::Node& node, State::Address addr)
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

void OSSIADevice::removeNode(const State::Address& address)
{
    using namespace OSSIA;
    if(!m_capas.canRemoveNode)
        return;
    if(!connected())
        return;

    OSSIA::Node* node = getNodeFromPath(address.path, m_dev.get());
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
        parent->erase(it);
    }
}

Device::Node OSSIADevice::refresh()
{
    Device::Node device_node{settings(), nullptr};

    if(!connected())
    {
        return device_node;
    }
    else
    {
        // Clear the listening
        removeListening_impl(*m_dev.get(), State::Address{m_settings.name, {}});

        if(m_dev->updateNamespace())
        {
            // Make a device explorer node from the current state of the device.
            // First make the node corresponding to the root node.

            // Recurse on the children
            auto& children = m_dev->children();
            device_node.reserve(children.size());
            for(const auto& node : children)
            {
                device_node.push_back(Ossia::convert::ToDeviceExplorer(*node.get()));
            }
        }
    }

    device_node.get<Device::DeviceSettings>().name = settings().name;

    return device_node;
}

boost::optional<State::Value> OSSIADevice::refresh(const State::Address& address)
{
    if(!connected())
        return {};

    auto node = findNodeFromPath(address.path, m_dev.get());
    if(node)
    {
        if(auto addr = node->getAddress())
            return Ossia::convert::ToValue(addr->pullValue());
    }

    return {};
}

Device::Node OSSIADevice::getNode(const State::Address& address)
{
    auto ossia_node = iscore::convert::findNodeFromPath(address.path, m_dev);
    if(ossia_node)
        return Ossia::convert::ToDeviceExplorer(*ossia_node.get());
    return {};
}

void OSSIADevice::setListening(
        const State::Address& addr,
        bool b)
{
    if(!connected())
        return;

    // First check if the address is already listening
    // so that we don't have to go through the tree.
    auto cb_it = m_callbacks.find(addr);

    std::shared_ptr<OSSIA::Address> ossia_addr;
    if(cb_it == m_callbacks.end())
    {
        auto n = findNodeFromPath(addr.path, m_dev.get());
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

    // If we want to enable listening
    // and the address wasn't already listening
    if(b)
    {
        if(cb_it == m_callbacks.end())
        {
            m_callbacks.insert(
            {
                addr,
                {
                     ossia_addr,
                     ossia_addr->addCallback([=] (const OSSIA::Value* val)
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

void OSSIADevice::replaceListening(const std::vector<State::Address>& addresses)
{
    if(!connected())
        return;

    for(const auto& elt : m_callbacks)
    {
        // addr -> removeCallback( callback );
        elt.second.first->removeCallback(elt.second.second);
    }
    m_callbacks.clear();

    for(const auto& addr : addresses)
    {
        ISCORE_ASSERT(addr.device == this->settings().name);
        setListening(addr, true);
    }
}


void OSSIADevice::sendMessage(const State::Message& mess)
{
    if(!connected())
        return;

    auto node = getNodeFromPath(mess.address.path, m_dev.get());

    auto addr = node->getAddress();
    if(addr)
        addr->pushValue(iscore::convert::toOSSIAValue(mess.value));
}


bool OSSIADevice::check(const QString &str)
{
    if(!connected())
        return false;

    ISCORE_TODO;
    return false;
}

OSSIA::Device& OSSIADevice::impl() const
{
    ISCORE_ASSERT(connected());
    return *m_dev;
}
}
