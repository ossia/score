#include <eggs/variant/variant.hpp>
#include <QByteArray>
#include <QDebug>
#include <vector>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include "DeviceNode.hpp"
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Device
{
QString DeviceExplorerNode::displayName() const
{
    static const constexpr struct {
        public:
            using return_type = QString;
            return_type operator()(const Device::DeviceSettings& dev) const
            { return dev.name; }

            return_type operator()(const Device::AddressSettings& addr) const
            { return addr.name; }

            return_type operator()(InvisibleRootNodeTag) const
            { return "Invisible Root DeviceExplorerNode"; }
    } visitor{};

    return eggs::variants::apply(visitor, impl());
}

bool DeviceExplorerNode::isSelectable() const
{
    return true;
}

bool DeviceExplorerNode::isEditable() const
{
    return is<Device::AddressSettings>() && hasOutput(get<Device::AddressSettings>().ioType);
}

Device::Node* getNodeFromString(
        Device::Node& n,
        QStringList&& parts)
{
    auto theN = try_getNodeFromString(n, std::move(parts));
    ISCORE_ASSERT(theN);
    return theN;
}

Device::Node& getNodeFromAddress(
        Device::Node& n,
        const State::Address& addr)
{
    auto theN = try_getNodeFromAddress(n, addr);
    ISCORE_ASSERT(theN);
    return *theN;
}

State::Address address(const Node &treeNode)
{
    State::Address addr;
    const Node* n = &treeNode;
    while(n->parent() && !n->is<DeviceSettings>())
    {
        addr.path.prepend(n->get<AddressSettings>().name);
        n = n->parent();
    }

    ISCORE_ASSERT(n);
    ISCORE_ASSERT(n->is<DeviceSettings>());
    addr.device = n->get<DeviceSettings>().name;

    return addr;
}

void parametersList(
        const Node& n,
        State::MessageList& ml)
{
    if(n.is<AddressSettings>())
    {
        const auto& stgs = n.get<AddressSettings>();

        if(stgs.ioType == IOType::InOut)
        {
            ml.push_back(message(n));
        }
    }

    for(const auto& child : n.children())
    {
        parametersList(child, ml);
    }
}

State::Message message(const Node& node)
{
    if(!node.is<Device::AddressSettings>())
        return {};

    State::Message mess;
    mess.address = address(node);
    mess.value = node.get<Device::AddressSettings>().value;

    return mess;
}


// TESTME
// TODO : this is really a pattern
// (see iscore2OSSIA, iscore_plugin_coppa and friends), try to refactor it.
// This could be a try_insert algorithm.
void merge(
        Device::Node& base,
        const State::Message& message)
{
    using Device::Node;

    QStringList path = message.address.path;
    path.prepend(message.address.device);

    ptr<Node> node = &base;
    for(int i = 0; i < path.size(); i++)
    {
        auto it = std::find_if(
                    node->begin(), node->end(),
                    [&] (const auto& cur_node) {
            return cur_node.displayName() == path[i];
        });

        if(it == node->end())
        {
            // We have to start adding sub-nodes from here.
            ptr<Node> parentnode{node};
            for(int k = i; k < path.size(); k++)
            {
                ptr<Node> newNode;
                if(k == 0)
                {
                    // We're adding a device
                    Device::DeviceSettings dev;
                    dev.name = path[k];
                    newNode = &parentnode->emplace_back(std::move(dev), nullptr);
                }
                else
                {
                    // We're adding an address
                    Device::AddressSettings addr;
                    addr.name = path[k];

                    if(k == path.size() - 1)
                    {
                        // End of the address
                        addr.value = message.value;

                        // Note : since we don't have this
                        // information in messagelist's,
                        // we assign a default Out value
                        // so that we only send the nodes that actually had messages
                        // via the OSSIA api.
                        addr.ioType = IOType::Out;
                    }

                    newNode = &parentnode->emplace_back(std::move(addr), nullptr);
                }

                // TODO do similar simplification on other similar algorithms
                // cf in ossia stuff
                parentnode = newNode;
            }

            break;
        }
        else
        {
            node = &*it;

            if(i == path.size() - 1)
            {
                // We replace the value by the one in the message
                if(node->is<Device::AddressSettings>())
                {
                    node->get<Device::AddressSettings>().value = message.value;
                }
            }
        }
    }
}

Device::Node merge(
        Device::Node base,
        const State::MessageList& other)
{
    using namespace iscore;
    // For each node in other, if we can also find a similar node in
    // base, we replace its data
    // Else, we insert it.

    ISCORE_ASSERT(base.is<InvisibleRootNodeTag>());

    for(const auto& message : other)
    {
        merge(base, message);
    }

    return base;
}

void dumpTree(const Node& node, QString rec)
{
    qDebug() << rec.toUtf8().constData() << node.displayName().toUtf8().constData();
    rec += " ";
    for(const auto& child : node)
    {
        dumpTree(child, rec);
    }
}

}

