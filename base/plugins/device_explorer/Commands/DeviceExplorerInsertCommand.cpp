
#include "DeviceExplorerInsertCommand.hpp"


DeviceExplorerInsertCommand::DeviceExplorerInsertCommand()
  : iscore::SerializableCommand("", "Insert ", "")
{
  
}

void
DeviceExplorerInsertCommand::set(const QModelIndex &parentIndex, int row, 
			      const QByteArray &data,
			      const QString &text,
			      DeviceExplorerModel *model)
{
  Q_ASSERT(model);
  m_model = model;
  m_data = data;
  m_parentPath = model->pathFromIndex(parentIndex);
  m_row = row;
  
  setText(text);
}

  
void
DeviceExplorerInsertCommand::undo()
{
  Q_ASSERT(m_model);

  QModelIndex parentIndex = m_model->pathToIndex(m_parentPath);

  const bool result = m_model->removeRows(m_row, 1, parentIndex);

  m_model->setCachedResult(result);

}

void
DeviceExplorerInsertCommand::redo()
{
  Q_ASSERT(m_model);
  QModelIndex parentIndex = m_model->pathToIndex(m_parentPath);
  
  const bool result = m_model->insertTreeData(parentIndex, m_row, m_data);
  m_model->setCachedResult(result);
}

int
DeviceExplorerInsertCommand::id() const
{
  return -1;
}

bool
DeviceExplorerInsertCommand::mergeWith(const QUndoCommand */*other*/)
{
  return false;
}


void
DeviceExplorerInsertCommand::serializeImpl(QDataStream &d)
{
  //TODO: should we pass the model ? how ???
  //TODO: should we serialize text() or is it serialized by base class ?

  DeviceExplorerModel::serializePath(d, m_parentPath);
  d << (qint32)m_row;
  
  d << (qint32)m_data.size();
  d.writeRawData(m_data.data(), m_data.size());

}

void
DeviceExplorerInsertCommand::deserializeImpl(QDataStream &d)
{
  //TODO: should we pass the model ? how ???
  //TODO: should we serialize text() or is it serialized by base class ?

  DeviceExplorerModel::deserializePath(d, m_parentPath);
  qint32 v;
  d >> v;
  m_row = v;

  d >> v;
  int size = v;
  m_data.resize(size);
  d.readRawData(m_data.data(), size);
}
