#include "EditData.hpp"

using namespace DeviceExplorer::Command;

// TODO fix this to use NodeUpdateProxy
EditData::EditData(
        Path<DeviceExplorerModel> &&device_tree,
        const iscore::NodePath& nodePath,
        DeviceExplorerModel::Column column,
        const QVariant& value,
        int role):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{device_tree},
    m_nodePath{nodePath},
    m_column{column},
    m_newValue{value},
    m_role{role}
{
    auto& explorer = m_deviceTree.find();
    m_oldValue = explorer.getData(m_nodePath, column, role);
}

void EditData::undo()
{
    auto& explorer = m_deviceTree.find();
    explorer.editData(m_nodePath, m_column, m_oldValue, m_role);
}

void EditData::redo()
{
    auto& explorer = m_deviceTree.find();
    explorer.editData(m_nodePath, m_column, m_newValue, m_role);
}

void EditData::serializeImpl(QDataStream &d) const
{
    d << m_deviceTree
      << m_nodePath;
    d << (int)m_column;
    d << m_oldValue;
    d << m_newValue;
    d << m_role;
}

void EditData::deserializeImpl(QDataStream &d)
{
    d >> m_deviceTree
      >> m_nodePath;
    d >> (int&)m_column;
    d >> m_oldValue;
    d >> m_newValue;
    d >> m_role;
}
