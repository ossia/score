
#include "DeviceExplorerMoveCommand.hpp"

#include <iostream> //DEBUG

DeviceExplorerMoveCommand::DeviceExplorerMoveCommand()
  : iscore::SerializableCommand("", "Move ", "")
{
  
}

void
DeviceExplorerMoveCommand::set(const QModelIndex &srcParentIndex, int srcRow, int count,
			    const QModelIndex &dstParentIndex, int dstRow,
			    const QString &text,
			    DeviceExplorerModel *model)
{
  Q_ASSERT(model);
  m_model = model;
  m_srcParentPath = model->pathFromIndex(srcParentIndex);
  m_dstParentPath = model->pathFromIndex(dstParentIndex);;
  m_srcRow = srcRow;
  m_dstRow = dstRow;
  m_count = count;

  setText(text);
}

  
void
DeviceExplorerMoveCommand::undo()
{
  Q_ASSERT(m_model);

  QModelIndex srcParentIndex = m_model->pathToIndex(m_srcParentPath);
  QModelIndex dstParentIndex = m_model->pathToIndex(m_dstParentPath);

  const bool result = m_model->undoMoveRows(srcParentIndex, m_srcRow, m_count, dstParentIndex, m_dstRow);
  m_model->setCachedResult(result);

}

void
DeviceExplorerMoveCommand::redo()
{
  Q_ASSERT(m_model);
  QModelIndex srcParentIndex = m_model->pathToIndex(m_srcParentPath);
  QModelIndex dstParentIndex = m_model->pathToIndex(m_dstParentPath);
  
  const bool result = m_model->moveRows(srcParentIndex, m_srcRow, m_count, dstParentIndex, m_dstRow);
  m_model->setCachedResult(result);
}

int
DeviceExplorerMoveCommand::id() const
{
  return -1;
}

bool
DeviceExplorerMoveCommand::mergeWith(const QUndoCommand */*other*/)
{
  return false;
}

void
DeviceExplorerMoveCommand::serializeImpl(QDataStream &d)
{
  //TODO: should we pass the model ? how ???
  //TODO: should we serialize text() or is it serialized by base class ?

  DeviceExplorerModel::serializePath(d, m_srcParentPath);
  DeviceExplorerModel::serializePath(d, m_dstParentPath);
  d << (qint32)m_srcRow;
  d << (qint32)m_dstRow;
  d << (qint32)m_count;
}

void
DeviceExplorerMoveCommand::deserializeImpl(QDataStream &d)
{
  //TODO: should we pass the model ? how ???
  //TODO: should we serialize text() or is it serialized by base class ?

  DeviceExplorerModel::deserializePath(d, m_srcParentPath);
  DeviceExplorerModel::deserializePath(d, m_dstParentPath);
  qint32 v;
  d >> v;
  m_srcRow = v;
  d >> v;
  m_dstRow = v;
  d >> v;
  m_count = v;
}
