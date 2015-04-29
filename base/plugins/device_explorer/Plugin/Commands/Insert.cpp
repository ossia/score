
#include "Insert.hpp"

using namespace DeviceExplorer::Command;

Insert::Insert()
    : iscore::SerializableCommand("", "Insert ", "")
{

}

void
Insert::set(const Path &parentPath, int row,
                                 const QByteArray& data,
                                 const QString& text,
                                 ObjectPath&& modelPath)
{
    m_model = modelPath;
    m_data = data;
    m_parentPath = parentPath;
    m_row = row;

    setText(text);
}


void
Insert::undo()
{
    auto model = m_model.find<DeviceExplorerModel>();
    Q_ASSERT(model);

    QModelIndex parentIndex = model->pathToIndex(m_parentPath);

    const bool result = model->removeRows(m_row, 1, parentIndex);

    model->setCachedResult(result);

}

void
Insert::redo()
{
    auto model = m_model.find<DeviceExplorerModel>();
    Q_ASSERT(model);
    QModelIndex parentIndex = model->pathToIndex(m_parentPath);

    const bool result = model->insertTreeData(parentIndex, m_row, m_data);
    model->setCachedResult(result);
}

bool
Insert::mergeWith(const Command* /*other*/)
{
    return false;
}


void
Insert::serializeImpl(QDataStream& d) const
{
    d << m_model;
    m_parentPath.serializePath(d);
    d << (qint32) m_row;

    d << (qint32) m_data.size();
    d.writeRawData(m_data.data(), m_data.size());

}

void
Insert::deserializeImpl(QDataStream& d)
{
    d >> m_model;
    m_parentPath.deserializePath(d);
    qint32 v;
    d >> v;
    m_row = v;

    d >> v;
    int size = v;
    m_data.resize(size);
    d.readRawData(m_data.data(), size);
}
