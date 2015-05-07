#include "UpdateNamespace.hpp"

using namespace DeviceExplorer::Command;

ReplaceDevice::ReplaceDevice(ObjectPath&& device_tree,
                             int deviceIndex,
                             Node rootNode):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                className(),
                                description()},
    m_deviceTree{device_tree},
    m_deviceIndex(deviceIndex),
    m_deviceNode{rootNode}
{

}

void ReplaceDevice::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    explorer.removeRow(m_deviceIndex);
    explorer.addDevice(new Node(m_savedNode));
}

void ReplaceDevice::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    explorer.removeRow(m_deviceIndex);
    explorer.addDevice(new Node(m_deviceNode));
}

void ReplaceDevice::serializeImpl(QDataStream& d) const
{
    d << m_deviceTree << m_deviceIndex << m_deviceNode << m_savedNode;
}

void ReplaceDevice::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree >> m_deviceIndex >> m_deviceNode >> m_savedNode;
}
