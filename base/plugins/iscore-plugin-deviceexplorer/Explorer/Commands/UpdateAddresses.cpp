#include "UpdateAddresses.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

using namespace DeviceExplorer::Command;
using namespace iscore;

// TODO this should not be a command, values can be updated without undo-ability.
UpdateAddressesValues::UpdateAddressesValues(
        Path<DeviceExplorerModel>&& device_tree,
        const QList<QPair<const Node *, iscore::Value> > &nodes):
    m_deviceTree{std::move(device_tree)}
{
    for(const auto& elt : nodes)
    {
        ISCORE_ASSERT(!elt.first->is<DeviceSettings>());
        m_data.append({*elt.first, {elt.first->get<AddressSettings>().value, elt.second}});
    }
}

void UpdateAddressesValues::undo() const
{
    auto& explorer = m_deviceTree.find();
    for(const auto& elt : m_data)
        explorer.editData(elt.first, DeviceExplorerModel::Column::Value, elt.second.first, Qt::EditRole);
}

void UpdateAddressesValues::redo() const
{
    auto& explorer = m_deviceTree.find();
    for(const auto& elt : m_data)
        explorer.editData(elt.first, DeviceExplorerModel::Column::Value, elt.second.second, Qt::EditRole);
}

void UpdateAddressesValues::serializeImpl(QDataStream& d) const
{
    d << m_deviceTree << m_data;
}

void UpdateAddressesValues::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree >> m_data;
}
