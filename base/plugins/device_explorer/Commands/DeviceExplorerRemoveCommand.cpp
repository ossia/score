
#include "DeviceExplorerRemoveCommand.hpp"

#include <iostream> //DEBUG

DeviceExplorerRemoveCommand::DeviceExplorerRemoveCommand()
    : iscore::SerializableCommand ("", "Remove ", "")
{

}

void
DeviceExplorerRemoveCommand::set (const QModelIndex& parentIndex, int row,
                                  const QByteArray& data,
                                  const QString& text,
                                  DeviceExplorerModel* model)
{
    Q_ASSERT (model);
    m_model = model;
    m_data = data;
    m_parentPath = model->pathFromIndex (parentIndex);
    m_row = row;

    setText (text);
}


void
DeviceExplorerRemoveCommand::undo()
{
    Q_ASSERT (m_model);

    QModelIndex parentIndex = m_model->pathToIndex (m_parentPath);

    const bool result = m_model->insertTreeData (parentIndex, m_row, m_data);
    m_model->setCachedResult (result);
}

void
DeviceExplorerRemoveCommand::redo()
{
    Q_ASSERT (m_model);
    QModelIndex parentIndex = m_model->pathToIndex (m_parentPath);

    QModelIndex index = parentIndex.child (m_row, 0);
    const bool resultG = m_model->getTreeData (index, m_data);

    if (! resultG)
    {
        m_model->setCachedResult (resultG);
        return;
    }

    const bool result = m_model->removeRows (m_row, 1, parentIndex);
    m_model->setCachedResult (result);
}

int
DeviceExplorerRemoveCommand::id() const
{
    return -1;
}

bool
DeviceExplorerRemoveCommand::mergeWith (const QUndoCommand* /*other*/)
{
    return false;
}


void
DeviceExplorerRemoveCommand::serializeImpl (QDataStream& d) const
{
    //TODO: should we pass the model ? how ???
    //TODO: should we serialize text() or is it serialized by base class ?

    DeviceExplorerModel::serializePath (d, m_parentPath);
    d << (qint32) m_row;

    d << (qint32) m_data.size();
    d.writeRawData (m_data.data(), m_data.size() );

}

void
DeviceExplorerRemoveCommand::deserializeImpl (QDataStream& d)
{
    //TODO: should we pass the model ? how ???
    //TODO: should we serialize text() or is it serialized by base class ?

    DeviceExplorerModel::deserializePath (d, m_parentPath);
    qint32 v;
    d >> v;
    m_row = v;

    d >> v;
    int size = v;
    m_data.resize (size);
    d.readRawData (m_data.data(), size);
}
