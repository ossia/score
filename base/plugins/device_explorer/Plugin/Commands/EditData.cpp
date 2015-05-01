#include "EditData.hpp"

using namespace DeviceExplorer::Command;

const char* EditData::className() { return "EditData"; }
QString EditData::description() { return "Edit data"; }

EditData::EditData(ObjectPath &&device_tree, Path nodePath, int column, QVariant value, int role):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                className(),
                                description()},
    m_deviceTree{device_tree},
    m_nodePath{nodePath},
    m_column{column},
    m_newValue{value},
    m_role{role}
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    m_oldValue = explorer->getData(m_nodePath, column, role);
}

void EditData::undo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    explorer->editData(m_nodePath, m_column, m_oldValue, m_role);
}

void EditData::redo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    explorer->editData(m_nodePath, m_column, m_newValue, m_role);
}

void EditData::serializeImpl(QDataStream &d) const
{
    d << m_deviceTree;
    m_nodePath.serializePath(d);
    d << m_column;
    d << m_oldValue; // TODO QVariants serialization
    d << m_newValue;
    d << m_role;
}

void EditData::deserializeImpl(QDataStream &d)
{
    d >> m_deviceTree;
    m_nodePath.deserializePath(d);
    d >> m_column;
    d >> m_oldValue;
    d >> m_newValue;
    d >> m_role;
}
