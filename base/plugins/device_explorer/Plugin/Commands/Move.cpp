
#include "Move.hpp"

#include <iostream> //DEBUG

using namespace DeviceExplorer::Command;

Move::Move()
    : iscore::SerializableCommand("", "Move ", "")
{

}

void
Move::set(const QModelIndex& srcParentIndex, int srcRow, int count,
                               const QModelIndex& dstParentIndex, int dstRow,
                               const QString& text,
                               DeviceExplorerModel* model)
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
Move::undo()
{
    Q_ASSERT(m_model);

    QModelIndex srcParentIndex = m_model->pathToIndex(m_srcParentPath);
    QModelIndex dstParentIndex = m_model->pathToIndex(m_dstParentPath);

    const bool result = m_model->undoMoveRows(srcParentIndex, m_srcRow, m_count, dstParentIndex, m_dstRow);
    m_model->setCachedResult(result);

}

void
Move::redo()
{
    Q_ASSERT(m_model);
    QModelIndex srcParentIndex = m_model->pathToIndex(m_srcParentPath);
    QModelIndex dstParentIndex = m_model->pathToIndex(m_dstParentPath);

    const bool result = m_model->moveRows(srcParentIndex, m_srcRow, m_count, dstParentIndex, m_dstRow);
    m_model->setCachedResult(result);
}

bool
Move::mergeWith(const Command* /*other*/)
{
    return false;
}

void
Move::serializeImpl(QDataStream& d) const
{
    DeviceExplorerModel::serializePath(d, m_srcParentPath);
    DeviceExplorerModel::serializePath(d, m_dstParentPath);
    d << (qint32) m_srcRow;
    d << (qint32) m_dstRow;
    d << (qint32) m_count;
}

void
Move::deserializeImpl(QDataStream& d)
{
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
