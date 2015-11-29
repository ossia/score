#include <Network/Node.h>
#include <qdebug.h>
#include <qstringlist.h>
#include <algorithm>
#include <list>

#include "Device/Address/AddressSettings.hpp"
#include "Device/Address/IOType.hpp"
#include "Device/Protocol/DeviceSettings.hpp"
#include "Network/Address.h"
#include "Network/Device.h"
#include "OSSIA2iscore.hpp"
#include "OSSIADevice.hpp"
#include "State/Message.hpp"
#include "State/Value.hpp"
#include "iscore2OSSIA.hpp"

namespace OSSIA {
class Value;
}  // namespace OSSIA

using namespace iscore::convert;
using namespace OSSIA::convert;

OSSIADevice::~OSSIADevice()
{

}

bool OSSIADevice::connected() const
{
    return bool(m_dev);
}

void OSSIADevice::updateSettings(const iscore::DeviceSettings& newsettings)
{
    // TODO we have to maintain the prior connection state
    // if we were disconnected, we stay disconnected
    // else we reconnect. See in Minuit / MIDI also.
    if(connected())
    {
        // First we save the existing nodes.
        iscore::Node iscore_device{settings(), nullptr};

        // Recurse on the children
        auto& ossia_children = m_dev->children();
        iscore_device.reserve(ossia_children.size());
        for(const auto& node : ossia_children)
        {
            iscore_device.push_back(OSSIA::convert::ToDeviceExplorer(*node.get()));
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
    m_callbacks.clear();

    m_dev.reset();
}

void OSSIADevice::addAddress(const iscore::FullAddressSettings &settings)
{
    using namespace OSSIA;
    if(!connected())
        return;

    // Create the node. It is added into the device.
    OSSIA::Node* node = createNodeFromPath(settings.address.path, m_dev.get());

    // Populate the node with an address (if it isn't a no_value_t).
    createOSSIAAddress(settings, node);
}


void OSSIADevice::updateAddress(const iscore::FullAddressSettings &settings)
{
    using namespace OSSIA;
    if(!connected())
        return;

    OSSIA::Node* node = getNodeFromPath(settings.address.path, m_dev.get());
    node->setName(settings.address.path.last().toStdString());

    if(settings.ioType == iscore::IOType::Invalid)
        removeOSSIAAddress(node);
    else
        updateOSSIAAddress(settings, node->getAddress());
}


void OSSIADevice::removeNode(const iscore::Address& address)
{
    using namespace OSSIA;
    if(!connected())
        return;

    OSSIA::Node* node = getNodeFromPath(address.path, m_dev.get());
    auto& children = node->getParent()->children();
    auto it = std::find_if(children.begin(), children.end(),
                           [&] (auto&& elt) { return elt.get() == node; });
    if(it != children.end())
    {
        children.erase(it);
    }
}

iscore::Node OSSIADevice::refresh()
{
    iscore::Node device_node{settings(), nullptr};

    if(!connected())
        return device_node;

    if(m_dev && m_dev->updateNamespace())
    {
        // Make a device explorer node from the current state of the device.
        // First make the node corresponding to the root node.

        // Recurse on the children
        auto& children = m_dev->children();
        device_node.reserve(children.size());
        for(const auto& node : children)
        {
            device_node.push_back(OSSIA::convert::ToDeviceExplorer(*node.get()));
        }
    }

    device_node.get<iscore::DeviceSettings>().name = settings().name;

    return device_node;
}

boost::optional<iscore::Value> OSSIADevice::refresh(const iscore::Address& address)
{
    if(!connected())
        return {};

    OSSIA::Node* node = getNodeFromPath(address.path, m_dev.get());
    ISCORE_ASSERT(node);

    if(auto addr = node->getAddress())
        return ToValue(addr->pullValue());

    return {};
}

void OSSIADevice::setListening(
        const iscore::Address& addr,
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
        auto n = getNodeFromPath(addr.path, m_dev.get());
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
                      { emit valueUpdated(addr, OSSIA::convert::ToValue(val)); })
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

std::vector<iscore::Address> OSSIADevice::listening() const
{
    if(!connected())
        return {};

    std::vector<iscore::Address> addrs;
    addrs.reserve(m_callbacks.size());

    for(const auto& elt : m_callbacks)
    {
        addrs.push_back(elt.first);
    }

    return addrs;
}

void OSSIADevice::replaceListening(const std::vector<iscore::Address>& addresses)
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


void OSSIADevice::sendMessage(const iscore::Message& mess)
{
    if(!connected())
        return;

    auto node = getNodeFromPath(mess.address.path, m_dev.get());

    auto addr = node->getAddress();
    if(addr)
        iscore::convert::setValue(*addr, mess.value);
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

