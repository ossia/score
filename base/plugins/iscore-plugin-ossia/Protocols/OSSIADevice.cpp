#include "OSSIADevice.hpp"
#include <QDebug>
#include <boost/range/algorithm.hpp>
#include <Network/Node.h>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"
using namespace iscore::convert;
using namespace OSSIA::convert;


void OSSIADevice::addAddress(const iscore::FullAddressSettings &settings)
{
    using namespace OSSIA;

    // Create the node. It is added into the device.
    OSSIA::Node* node = createNodeFromPath(settings.address.path, m_dev.get());

    // Populate the node with an address. Won't add one if IOType == Invalid.
    createOSSIAAddress(settings, node);
}


void OSSIADevice::updateAddress(const iscore::FullAddressSettings &settings)
{
    using namespace OSSIA;

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

    if(m_dev && m_dev->updateNamespace())
    {
        // Make a device explorer node from the current state of the device.
        // First make the node corresponding to the root node.

        iscore::Address addr;
        addr.device = settings().name;

        // Recurse on the children
        for(const auto& node : m_dev->children())
        {
            device_node.push_back(OSSIAToDeviceExplorer(*node.get(), addr));
        }
    }

    device_node.get<iscore::DeviceSettings>().name = settings().name;

    return device_node;
}

boost::optional<iscore::Value> OSSIADevice::refresh(const iscore::Address& address)
{
    OSSIA::Node* node = getNodeFromPath(address.path, m_dev.get());
    ISCORE_ASSERT(node);

    if(auto addr = node->getAddress())
        return ToValue(addr->pullValue());

    return {};
}

void OSSIADevice::setListening(const iscore::Address& addr, bool b)
{
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


void OSSIADevice::sendMessage(iscore::Message mess)
{
    auto node = getNodeFromPath(mess.address.path, m_dev.get());

    auto addr = node->getAddress();
    auto val = addr->getValue()->clone();
    updateOSSIAValue(mess.value.val, *val);
    addr->pushValue(val);
}


bool OSSIADevice::check(const QString &str)
{
    ISCORE_TODO;
    return false;
}

OSSIA::Device& OSSIADevice::impl() const
{
    return *m_dev;
}

iscore::Node OSSIADevice::OSSIAToDeviceExplorer(const OSSIA::Node& node, iscore::Address currentAddr)
{
    iscore::Node n{ToAddressSettings(node), nullptr};

    currentAddr.path += n.get<iscore::AddressSettings>().name;

    /*
    // 2. Add a callback
    if(n.get<iscore::AddressSettings>().ioType != iscore::IOType::Invalid)
    {
        if(auto ossia_addr = node.getAddress())
        {
            m_callbacks.insert({
                        currentAddr,
                        ossia_addr->addCallback([=] (const OSSIA::Value* val) {
                emit valueUpdated(currentAddr, OSSIA::convert::ToValue(val));
            })});
        }
    }
    */

    // 3. Recurse on the children
    for(const auto& ossia_child : node.children())
    {
        auto child_n = OSSIAToDeviceExplorer(*ossia_child.get(), currentAddr);
        child_n.setParent(&n);
        n.push_back(std::move(child_n));
    }

    return n;
}

