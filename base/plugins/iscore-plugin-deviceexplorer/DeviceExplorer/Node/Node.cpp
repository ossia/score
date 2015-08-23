#include "Node.hpp"

namespace iscore
{
QString DeviceExplorerNode::displayName() const
{
    switch(type())
    {
        case Type::Address:
            return addressSettings().name;
            break;
        case Type::Device:
            return deviceSettings().name;
            break;
        case Type::RootNode:
            return "Invisible Root DeviceExplorerNode";
            break;
        default:
            return "==ERROR==";
            break;
    }
}

bool DeviceExplorerNode::isSelectable() const
{
    return true;
}

bool DeviceExplorerNode::isEditable() const
{
    return type() == Type::Address
            && (addressSettings().ioType == IOType::InOut
               ||  addressSettings().ioType == IOType::Invalid);
}

bool DeviceExplorerNode::isDevice() const
{
    return type() == Type::Device;
}

void DeviceExplorerNode::setDeviceSettings(const DeviceSettings &settings)
{
    m_data = settings;
}

const DeviceSettings& DeviceExplorerNode::deviceSettings() const
{
    return eggs::variants::get<DeviceSettings>(m_data);
}

DeviceSettings& DeviceExplorerNode::deviceSettings()
{
    return eggs::variants::get<DeviceSettings>(m_data);
}

void DeviceExplorerNode::setAddressSettings(const iscore::AddressSettings &settings)
{
    m_data = settings;
}

const iscore::AddressSettings &DeviceExplorerNode::addressSettings() const
{
    return eggs::variants::get<AddressSettings>(m_data);
}

iscore::AddressSettings &DeviceExplorerNode::addressSettings()
{
    return eggs::variants::get<AddressSettings>(m_data);
}

iscore::Node* getNodeFromString(iscore::Node* n, QStringList&& parts)
{
    auto theN = try_getNodeFromString(n, std::move(parts));
    Q_ASSERT(theN);
    return theN;
}

iscore::Node* try_getNodeFromString(iscore::Node* n, QStringList&& parts)
{
    if(parts.size() == 0)
        return n;

    for(const auto& child : n->children())
    {
        if(child->displayName() == parts[0])
        {
            parts.removeFirst();
            return try_getNodeFromString(child, std::move(parts));
        }
    }

    return nullptr;
}

Address address(const Node &treeNode)
{
    Address addr;
    const Node* n = &treeNode;
    while(n->parent() && !n->isDevice())
    {
        addr.path.prepend(n->addressSettings().name);
        n = n->parent();
    }

    Q_ASSERT(n->isDevice());
    addr.device = n->deviceSettings().name;

    return addr;
}

}

