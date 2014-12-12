
#include "DeviceExplorerPasteCommand.hpp"


DeviceExplorerPasteCommand::DeviceExplorerPasteCommand()
  : iscore::SerializableCommand("", "Paste ", "")
{
  
}

void
DeviceExplorerPasteCommand::set(const QModelIndex &parentIndex, int row, 
			     const QString &text,
			     DeviceExplorerModel *model)
{
  Q_ASSERT(model);
  m_model = model;
  m_parentPath = model->pathFromIndex(parentIndex);
  m_row = row;
  
  setText(text);
}

  
void
DeviceExplorerPasteCommand::undo()
{
  Q_ASSERT(m_model);

  QModelIndex parentIndex = m_model->pathToIndex(m_parentPath);

  QModelIndex index = parentIndex.child(m_row+1, 0);//+1 because pasteAfter
  const DeviceExplorerModel::Result result = m_model->cut_aux(index);
  m_model->setCachedResult(result);

}

void
DeviceExplorerPasteCommand::redo()
{
  Q_ASSERT(m_model);
  QModelIndex parentIndex = m_model->pathToIndex(m_parentPath);
  
  QModelIndex index = parentIndex.child(m_row, 0);
  const DeviceExplorerModel::Result result = m_model->pasteAfter_aux(index);
  m_model->setCachedResult(result);

}

int
DeviceExplorerPasteCommand::id() const
{
  return -1;
}

bool
DeviceExplorerPasteCommand::mergeWith(const QUndoCommand */*other*/)
{
  return false;
}


void
DeviceExplorerPasteCommand::serializeImpl(QDataStream &d)
{
  //TODO: should we pass the model ? how ???
  //TODO: should we serialize text() or is it serialized by base class ?

  DeviceExplorerModel::serializePath(d, m_parentPath);
  d << (qint32)m_row;
  
  d << (qint32)m_data.size();
  d.writeRawData(m_data.data(), m_data.size());

}

void
DeviceExplorerPasteCommand::deserializeImpl(QDataStream &d)
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
