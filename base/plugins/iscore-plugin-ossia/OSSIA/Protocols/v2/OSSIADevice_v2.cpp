#include <Network/Node.h>
#include <QDebug>
#include <QStringList>
#include <algorithm>
#include <list>

#include <Device.hpp>
#include <Address.hpp>
#include <Protocol.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include "OSSIADevice_v2.hpp"
#include <Network/NetworkLogger.h>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

namespace iscore
{
namespace convert
{

static OSSIA::BoundingMode toBoundingMode(Device::ClipMode c)
{
    switch(c)
    {
        case Device::ClipMode::Free:
            return OSSIA::BoundingMode::FREE;
        case Device::ClipMode::Clip:
            return OSSIA::BoundingMode::CLIP;
        case Device::ClipMode::Wrap:
            return OSSIA::BoundingMode::WRAP;
        case Device::ClipMode::Fold:
            return OSSIA::BoundingMode::FOLD;
        default:
            ISCORE_ABORT;
            return static_cast<OSSIA::BoundingMode>(-1);
    }
}

void updateOSSIAAddress(
        const Device::FullAddressSettings &settings,
        OSSIA::v2::Address& addr)
{
    switch(settings.ioType)
    {
        case Device::IOType::In:
            addr.setAccessMode(OSSIA::AccessMode::GET);
            break;
        case Device::IOType::Out:
            addr.setAccessMode(OSSIA::AccessMode::SET);
            break;
        case Device::IOType::InOut:
            addr.setAccessMode(OSSIA::AccessMode::BI);
            break;
        case Device::IOType::Invalid:
            ISCORE_ABORT;
            break;
    }

    addr.setRepetitionFilter(settings.repetitionFilter);
    addr.setBoundingMode(iscore::convert::toBoundingMode(settings.clipMode));

    addr.setValue(iscore::convert::toOSSIAValue(settings.value));

    addr.setDomain(
                OSSIA::v2::makeDomain(
                    toOSSIAValue(settings.domain.min),
                    toOSSIAValue(settings.domain.max)));

}


void createOSSIAAddress(
        const Device::FullAddressSettings &settings,
        OSSIA::v2::Node *node)
{
    if(settings.value.val.is<State::no_value_t>())
        return;

    struct {
        public:
            using return_type = OSSIA::Type;
            return_type operator()(const State::no_value_t&) const { ISCORE_ABORT; return OSSIA::Type::IMPULSE; }
            return_type operator()(const State::impulse_t&) const { return OSSIA::Type::IMPULSE; }
            return_type operator()(int) const { return OSSIA::Type::INT; }
            return_type operator()(float) const { return OSSIA::Type::FLOAT; }
            return_type operator()(bool) const { return OSSIA::Type::BOOL; }
            return_type operator()(const QString&) const { return OSSIA::Type::STRING; }
            return_type operator()(const QChar&) const { return OSSIA::Type::CHAR; }
            return_type operator()(const State::vec2f&) const { return OSSIA::Type::VEC2F; }
            return_type operator()(const State::vec3f&) const { return OSSIA::Type::VEC3F; }
            return_type operator()(const State::vec4f&) const { return OSSIA::Type::VEC4F; }
            return_type operator()(const State::tuple_t&) const { return OSSIA::Type::TUPLE; }
    } visitor{};

    auto addr = node->createAddress(eggs::variants::apply(visitor, settings.value.val.impl()));
    if(addr)
        updateOSSIAAddress(settings, *addr);
}

OSSIA::v2::Node* findNodeFromPath(
        const QStringList& path, OSSIA::v2::Device& dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    OSSIA::v2::Node* node = &dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = std::find_if(children.begin(), children.end(),
                               [&] (const auto& ossia_node)
        {
            return ossia_node->getName() == path[i].toStdString();
        });
        if(it != children.end())
            node = it->get();
        else
            return {};
    }

    return node;
}

OSSIA::v2::Node* getNodeFromPath(
        const QStringList &path,
        OSSIA::v2::Device& dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    OSSIA::v2::Node* node = &dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();

        auto it = find_if(children, [&] (const auto& ossia_node)
        {
            return ossia_node->getName() == path[i].toStdString();
        });

        ISCORE_ASSERT(it != children.end());

        node = it->get();
    }

    ISCORE_ASSERT(node);
    return node;
}

}
}
namespace Ossia
{
namespace convert
{


OSSIA::v2::Node *createNodeFromPath(const QStringList &path, OSSIA::v2::Device *dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    OSSIA::v2::Node* node = dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = find_if(children,
                    [&] (const auto& ossia_node) { return ossia_node->getName() == path[i].toStdString(); });

        if(it == children.end())
        {
            // We have to start adding sub-nodes from here.
            OSSIA::v2::Node* parentnode = node;
            for(int k = i; k < path.size(); k++)
            {
                auto newNode = parentnode->createChild(path[k].toStdString());
                if(path[k].toStdString() != newNode->getName())
                {
                    qDebug() << path[k] << newNode->getName().c_str();
                    for(const auto& node : parentnode->children())
                    {
                        qDebug() << node->getName().c_str();
                    }
                    ISCORE_ABORT;
                }
                if(k == path.size() - 1)
                {
                    node = newNode;
                }
                else
                {
                    parentnode = newNode;
                }
            }

            break;
        }
        else
        {
            node = it->get();
        }
    }

    return node;
}

Device::Domain ToDomain(const OSSIA::v2::Domain &domain)
{
    Device::Domain d;
    d.min = ToValue(OSSIA::v2::min(domain));
    d.max = ToValue(OSSIA::v2::max(domain));

    ISCORE_TODO;
    // TODO values!!
    return d;
}

Device::AddressSettings ToAddressSettings(const OSSIA::v2::Node &node)
{
    Device::AddressSettings s;
    s.name = QString::fromStdString(node.getName());

    const auto& addr = node.getAddress();

    if(addr)
    {
        addr->pullValue();

        try {
            s.value = ToValue(addr->cloneValue());
        }
        catch(...)
        {
            s.value = ToValue(addr->getValueType());
        }

        s.ioType = ToIOType(addr->getAccessMode());
        s.clipMode = ToClipMode(addr->getBoundingMode());
        s.repetitionFilter = addr->getRepetitionFilter();

        if(auto& domain = addr->getDomain())
            s.domain = ToDomain(domain);
    }
    return s;
}
Device::Node ToDeviceExplorer(const OSSIA::v2::Node &ossia_node)
{
    Device::Node iscore_node{ToAddressSettings(ossia_node), nullptr};
    iscore_node.reserve(ossia_node.children().size());

    // 2. Recurse on the children
    for(const auto& ossia_child : ossia_node.children())
    {
        auto child_n = ToDeviceExplorer(*ossia_child.get());
        child_n.setParent(&iscore_node);
        iscore_node.push_back(std::move(child_n));
    }

    return iscore_node;
}

}
namespace Protocols
{
OSSIADevice_v2::~OSSIADevice_v2()
{
    if(connected())
        disconnect();
}

bool OSSIADevice_v2::connected() const
{
    return bool(m_dev);
}

void OSSIADevice_v2::updateSettings(const Device::DeviceSettings& newsettings)
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

void OSSIADevice_v2::disconnect()
{
    if(connected())
    {
        removeListening_impl(*m_dev.get(), State::Address{m_settings.name, {}});

        m_dev->clearChildren();
    }

    m_callbacks.clear();
    m_dev.reset();
    setLogging_impl(false);
}

void OSSIADevice_v2::addAddress(const Device::FullAddressSettings &settings)
{
    using namespace OSSIA;
    if(!m_capas.canAddNode)
        return; // TODO return bool instead, and check in the node update proxy ?

    if(!connected())
        return;

    // Create the node. It is added into the device.
    OSSIA::v2::Node* node = Ossia::convert::createNodeFromPath(settings.address.path, m_dev.get());

    // Populate the node with an address (if it isn't a no_value_t).
    iscore::convert::createOSSIAAddress(settings, node);
}


void OSSIADevice_v2::updateAddress(
        const State::Address& currentAddr,
        const Device::FullAddressSettings &settings)
{
    if(!connected())
        return;

    OSSIA::v2::Node* node = iscore::convert::getNodeFromPath(currentAddr.path, *m_dev);
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
            iscore::convert::createOSSIAAddress(settings, node);
    }
}

void OSSIADevice_v2::removeListening_impl(
        OSSIA::v2::Node& node, State::Address addr)
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

void OSSIADevice_v2::setLogging_impl(bool b) const
{
    if(!m_dev)
        return;

    if(b)
    {
        auto l = std::make_shared<OSSIA::NetworkLogger>();
        l->setInboundLogCallback([=] (std::string s) {
            emit logInbound(QString::fromStdString(s));
        });
        l->setOutboundLogCallback([=] (std::string s) {
            emit logOutbound(QString::fromStdString(s));
        });
        m_dev->getProtocol().setLogger(std::move(l));
        qDebug() << "logging enabled";
    }
    else
    {
        m_dev->getProtocol().setLogger({});
        qDebug() << "logging disabled";
    }
}

void OSSIADevice_v2::removeNode(const State::Address& address)
{
    using namespace OSSIA;
    if(!m_capas.canRemoveNode)
        return;
    if(!connected())
        return;

    OSSIA::v2::Node* node = iscore::convert::getNodeFromPath(address.path, *m_dev);
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

Device::Node OSSIADevice_v2::refresh()
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

optional<State::Value> OSSIADevice_v2::refresh(const State::Address& address)
{
    if(!connected())
        return {};

    auto node = iscore::convert::findNodeFromPath(address.path, *m_dev);
    if(node)
    {
        if(auto addr = node->getAddress())
        {
            addr->pullValue();
            return Ossia::convert::ToValue(addr->cloneValue());
        }
    }

    return {};
}

Device::Node OSSIADevice_v2::getNode(const State::Address& address)
{
    if(!m_dev)
        return {};

    auto ossia_node = iscore::convert::findNodeFromPath(address.path, *m_dev);
    if(ossia_node)
        return Ossia::convert::ToDeviceExplorer(*ossia_node);
    return {};
}

void OSSIADevice_v2::setListening(
        const State::Address& addr,
        bool b)
{
    if(!connected())
        return;

    // First check if the address is already listening
    // so that we don't have to go through the tree.
    auto cb_it = m_callbacks.find(addr);

    OSSIA::v2::Address* ossia_addr{};
    if(cb_it == m_callbacks.end())
    {
        auto n = iscore::convert::findNodeFromPath(addr.path, *m_dev);
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
                     ossia_addr->addCallback([=] (const OSSIA::Value& val)
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

std::vector<State::Address> OSSIADevice_v2::listening() const
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

void OSSIADevice_v2::addToListening(const std::vector<State::Address>& addresses)
{
    if(!connected())
        return;

    for(const auto& addr : addresses)
    {
        ISCORE_ASSERT(addr.device == this->settings().name);
        setListening(addr, true);
    }
}


void OSSIADevice_v2::sendMessage(const State::Message& mess)
{
    if(!connected())
        return;

    auto node = iscore::convert::getNodeFromPath(mess.address.path, *m_dev);

    auto addr = node->getAddress();
    if(addr)
    {
        addr->pushValue(iscore::convert::toOSSIAValue(mess.value));
    }
}

bool OSSIADevice_v2::isLogging() const
{
    return m_logging;
}

void OSSIADevice_v2::setLogging(bool b)
{
    if(!connected())
        return;

    if(b == m_logging)
        return;

    m_logging = b;
    setLogging_impl(m_logging);
}


OSSIA::v2::Device& OSSIADevice_v2::impl() const
{
    ISCORE_ASSERT(connected());
    return *m_dev;
}
}
}
