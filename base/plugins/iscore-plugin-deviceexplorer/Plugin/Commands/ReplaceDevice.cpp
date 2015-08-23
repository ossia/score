#include "ReplaceDevice.hpp"

using namespace DeviceExplorer::Command;
using namespace iscore;

ReplaceDevice::ReplaceDevice(ObjectPath&& device_tree,
                             int deviceIndex,
                             Node&& rootNode):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{device_tree},
    m_deviceIndex(deviceIndex),
    m_deviceNode{std::move(rootNode)}
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();
    m_savedNode = *explorer.nodeFromModelIndex(explorer.index(m_deviceIndex, 0, QModelIndex()));
}

void ReplaceDevice::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    explorer.removeRow(m_deviceIndex);
    explorer.addDevice(new Node{m_savedNode, nullptr});
}

void ReplaceDevice::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    explorer.removeRow(m_deviceIndex);
    explorer.addDevice(new Node{m_deviceNode, nullptr});
}

void ReplaceDevice::serializeImpl(QDataStream& d) const
{
    d << m_deviceTree << m_deviceIndex << m_deviceNode << m_savedNode;
}

void ReplaceDevice::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree >> m_deviceIndex >> m_deviceNode >> m_savedNode;
}
