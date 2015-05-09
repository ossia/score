#include "AddAddress.hpp"
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

using namespace DeviceExplorer::Command;

const char* AddAddress::className() { return "AddAddress"; }
QString AddAddress::description() { return QObject::tr("Add an address"); }

AddAddress::AddAddress(ObjectPath &&device_tree,
                       Path nodePath,
                       DeviceExplorerModel::Insert insert,
                       const AddressSettings &addressSettings):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                className(),
                                description()},
    m_deviceTree{device_tree}
{
    m_addressSettings = addressSettings;

    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    Node* parentNode{};
    if (insert == DeviceExplorerModel::Insert::AsChild)
    {
        parentNode =  nodePath.toNode(explorer.rootNode());
    }
    else if (insert == DeviceExplorerModel::Insert::AsSibling)
    {
        parentNode =  nodePath.toNode(explorer.rootNode())->parent();
    }
    m_parentNodePath = Path{parentNode};
}

void AddAddress::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();
    Node* parent = m_parentNodePath.toNode(explorer.rootNode());
    explorer.removeNode(parent->childAt(m_createdNodeIndex));
}

void AddAddress::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();
    auto parentnode = m_parentNodePath.toNode(explorer.rootNode());

    // Add in the device impl
    // Get the device node :
    auto dev_node = explorer.rootNode()->childAt(m_parentNodePath.at(0));

    // Make a full path (not taking the device into account.. for instance /MIDI.1/a/b would give /a/b)
    FullAddressSettings full = m_addressSettings;
    auto parentpath = parentnode->fullPath();
    parentpath.removeFirst();
    full.name = parentpath.join("/").prepend("/") + "/" + m_addressSettings.name;

    // Add in the device implementation
    explorer.deviceModel()->list().device(dev_node->name()).addAddress(full);

    // Add in the device explorer
    Node* newNode = explorer.addAddress( m_parentNodePath.toNode(explorer.rootNode()), m_addressSettings);

    m_createdNodeIndex = m_parentNodePath.toNode(explorer.rootNode())->indexOfChild(newNode);
}

void AddAddress::serializeImpl(QDataStream &s) const
{
    s << m_deviceTree << m_parentNodePath.toList() << m_addressSettings << m_createdNodeIndex;
}

void AddAddress::deserializeImpl(QDataStream &s)
{
    QList<int> path;
    s >> m_deviceTree >> path >> m_addressSettings >> m_createdNodeIndex;
    m_parentNodePath = path;
}
