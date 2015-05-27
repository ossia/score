
#include "Insert.hpp"

using namespace DeviceExplorer::Command;

Insert::Insert(const Path &parentPath,
               int row,
               Node &&data,
               ObjectPath&& modelPath):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                this->className(),
                                this->description()},
    m_model{std::move(modelPath)},
    m_node{std::move(data)},
    m_parentPath{parentPath},
    m_row{row}
{
    qDebug() << m_node.name();
}


void
Insert::undo()
{
    auto& model = m_model.find<DeviceExplorerModel>();

    QModelIndex parentIndex = model.pathToIndex(m_parentPath);

    const bool result = model.removeRows(m_row, 1, parentIndex);

    model.setCachedResult(result);
}

void
Insert::redo()
{
    auto& model = m_model.find<DeviceExplorerModel>();
    QModelIndex parentIndex = model.pathToIndex(m_parentPath);

    const bool result = model.insertNode(parentIndex, m_row, m_node);

    model.setCachedResult(result);
}

void
Insert::serializeImpl(QDataStream& d) const
{
    d << m_model << m_node << m_parentPath << m_row;
    /*
    // TODO keep this "read/write raw data" in mind for other places!!
    d << (qint32) m_data.size();
    d.writeRawData(m_data.data(), m_data.size());
    */
}

void
Insert::deserializeImpl(QDataStream& d)
{
    d >> m_model >> m_node >> m_parentPath >> m_row;
    /*
    d >> v;
    int size = v;
    m_data.resize(size);
    d.readRawData(m_data.data(), size);
    */
}
