#include "UpdateAddress.hpp"
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

using namespace DeviceExplorer::Command;
using namespace iscore;

UpdateAddresses::UpdateAddresses(
        ObjectPath&& device_tree,
        const QList<QPair<const Node *, iscore::Value> > &nodes):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{std::move(device_tree)}
{
    for(const auto& elt : nodes)
    {
        Q_ASSERT(!elt.first->isDevice());
        m_data.append({*elt.first, {elt.first->addressSettings().value, elt.second}});
    }
}

void UpdateAddresses::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();
    for(const auto& elt : m_data)
        explorer.editData(elt.first, DeviceExplorerModel::Column::Value, elt.second.first.val, Qt::EditRole);
}

void UpdateAddresses::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();
    for(const auto& elt : m_data)
        explorer.editData(elt.first, DeviceExplorerModel::Column::Value, elt.second.second.val, Qt::EditRole);
}

void UpdateAddresses::serializeImpl(QDataStream& d) const
{
    d << m_deviceTree << m_data;
}

void UpdateAddresses::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree >> m_data;
}
