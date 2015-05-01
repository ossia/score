
#include "Paste.hpp"

using namespace DeviceExplorer::Command;

Paste::Paste()
    : iscore::SerializableCommand("", "Paste ", "")
{

}

void
Paste::set(const Path &parentPath, int row,
                                const QString& text,
                                ObjectPath &&modelPath)
{
    m_model = modelPath;
    m_row = row;
    m_parentPath = parentPath;
    setText(text);
}


void
Paste::undo()
{
    auto model = m_model.find<DeviceExplorerModel>();
    Q_ASSERT(model);

    QModelIndex parentIndex = model->pathToIndex(m_parentPath);

    QModelIndex index = parentIndex.child(m_row + 1, 0);  //+1 because pasteAfter
    const DeviceExplorer::Result result = model->cut_aux(index);
    model->setCachedResult(result);

}

void
Paste::redo()
{
    auto model = m_model.find<DeviceExplorerModel>();
    Q_ASSERT(model);
    QModelIndex parentIndex = model->pathToIndex(m_parentPath);

    QModelIndex index = parentIndex.child(m_row, 0);
    const DeviceExplorer::Result result = model->pasteAfter_aux(index);
    model->setCachedResult(result);

}

void
Paste::serializeImpl(QDataStream& d) const
{
    d << m_model;
    m_parentPath.serializePath(d);
    d << (qint32) m_row;

    d << (qint32) m_data.size();
    d.writeRawData(m_data.data(), m_data.size());

}

void
Paste::deserializeImpl(QDataStream& d)
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
