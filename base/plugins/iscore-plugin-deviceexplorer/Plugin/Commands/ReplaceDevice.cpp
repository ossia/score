#include "ReplaceDevice.hpp"

using namespace DeviceExplorer::Command;
using namespace iscore;
// TODO fix this to use NodeUpdateProxy. Maybe it should be a Remove() followed by
// a LoadDevice() ?
ReplaceDevice::ReplaceDevice(Path<DeviceExplorerModel>&& device_tree,
                             int deviceIndex,
                             Node&& rootNode):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{device_tree},
    m_deviceIndex(deviceIndex),
    m_deviceNode{std::move(rootNode)}
{
    auto& explorer = m_deviceTree.find();
    m_savedNode = *explorer.nodeFromModelIndex(explorer.index(m_deviceIndex, 0, QModelIndex()));
}

void ReplaceDevice::undo() const
{
    auto& explorer = m_deviceTree.find();

    explorer.removeRow(m_deviceIndex);
    explorer.addDevice(m_savedNode);
}

void ReplaceDevice::redo() const
{
    auto& explorer = m_deviceTree.find();

    const auto& cld = explorer.rootNode().children();
    for(auto it = cld.begin(); it != cld.end(); ++it)
    {
        auto ds = it->get<iscore::DeviceSettings>();
        if(ds.name == m_deviceNode.get<iscore::DeviceSettings>().name)
        {
            explorer.removeNode(it);
            break;
        }
    }

    explorer.addDevice(m_deviceNode);
}

void ReplaceDevice::serializeImpl(QDataStream& d) const
{
    d << m_deviceTree << m_deviceIndex << m_deviceNode << m_savedNode;
}

void ReplaceDevice::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree >> m_deviceIndex >> m_deviceNode >> m_savedNode;
}
