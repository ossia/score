#include "EditData.hpp"

using namespace DeviceExplorer::Command;

const char* EditData::className() { return "EditData"; }
QString EditData::description() { return "Edit data"; }

EditData::EditData(ObjectPath &&device_tree, QModelIndex index, QVariant value, int role):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                className(),
                                description()},
    m_deviceTree{device_tree},
    m_index{index},
    m_newValue{value},
    m_role{role}
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    m_nodePath = explorer->pathFromIndex(index);
    m_oldValue = explorer->data(index, role);
}

void EditData::undo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    explorer->editData(m_index, m_oldValue, m_role);
}

void EditData::redo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    explorer->editData(m_index, m_newValue, m_role);
}

bool EditData::mergeWith(const iscore::Command *other)
{
    return false;
}

void EditData::serializeImpl(QDataStream &) const
{

}

void EditData::deserializeImpl(QDataStream &)
{

}
