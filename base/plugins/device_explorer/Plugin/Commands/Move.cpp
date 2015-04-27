
#include "Move.hpp"

using namespace DeviceExplorer::Command;

Move::Move()
    : iscore::SerializableCommand("", "Move ", "")
{

}

void
Move::set(const Path &srcParentPath, int srcRow, int count,
                               const Path &dstParentPath, int dstRow,
                               const QString& text,
                               ObjectPath &&tree_model)
{
    m_model = tree_model;
    m_srcParentPath = srcParentPath;
    m_dstParentPath = dstParentPath;
    m_srcRow = srcRow;
    m_dstRow = dstRow;
    m_count = count;

    setText(text);
}


void
Move::undo()
{
    auto model = m_model.find<DeviceExplorerModel>();
    Q_ASSERT(model);

    QModelIndex srcParentIndex = model->pathToIndex(m_srcParentPath);
    QModelIndex dstParentIndex = model->pathToIndex(m_dstParentPath);

    const bool result = model->undoMoveRows(srcParentIndex, m_srcRow, m_count, dstParentIndex, m_dstRow);
    model->setCachedResult(result);

}

void
Move::redo()
{
    auto model = m_model.find<DeviceExplorerModel>();
    Q_ASSERT(model);
    QModelIndex srcParentIndex = model->pathToIndex(m_srcParentPath);
    QModelIndex dstParentIndex = model->pathToIndex(m_dstParentPath);

    const bool result = model->moveRows(srcParentIndex, m_srcRow, m_count, dstParentIndex, m_dstRow);
    model->setCachedResult(result);
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
