#include <QDataStream>
#include <QtGlobal>
#include <qnamespace.h>
#include <algorithm>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include "UpdateAddresses.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/TreePath.hpp>

using namespace DeviceExplorer::Command;
using namespace iscore;

// TODO this should not be a command, values can be updated without undo-ability.
UpdateAddressesValues::UpdateAddressesValues(
        Path<DeviceExplorerModel>&& device_tree,
        const QList<QPair<const Device::Node *, State::Value> > &nodes):
    m_deviceTree{std::move(device_tree)}
{
    for(const auto& elt : nodes)
    {
        ISCORE_ASSERT(!elt.first->is<Device::DeviceSettings>());
        m_data.append({*elt.first, {elt.first->get<Device::AddressSettings>().value, elt.second}});
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

void UpdateAddressesValues::serializeImpl(DataStreamInput& d) const
{
    d << m_deviceTree << m_data;
}

void UpdateAddressesValues::deserializeImpl(DataStreamOutput& d)
{
    d >> m_deviceTree >> m_data;
}
