
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
    auto& model = m_model.find<DeviceExplorerModel>();

    QModelIndex srcParentIndex = model.convertPathToIndex(m_srcParentPath);
    QModelIndex dstParentIndex = model.convertPathToIndex(m_dstParentPath);

    int src = m_dstRow;
    int dst = m_srcRow;

    // There are pb when moving under a same parent :
    // all next items are changing row
    if (srcParentIndex == dstParentIndex)
    {
        if(m_srcRow > m_dstRow)
        {
            dst = m_srcRow + m_count;
        }
        else
        {
            src = m_dstRow - m_count;
        }
    }

    const bool result = model.moveRows(dstParentIndex, src, m_count, srcParentIndex, dst);

    model.setCachedResult(result);

}

void
Move::redo()
{
    auto& model = m_model.find<DeviceExplorerModel>();

    QModelIndex srcParentIndex = model.convertPathToIndex(m_srcParentPath);
    QModelIndex dstParentIndex = model.convertPathToIndex(m_dstParentPath);

    const bool result = model.moveRows(srcParentIndex, m_srcRow, m_count, dstParentIndex, m_dstRow);
    model.setCachedResult(result);
}

void
Move::serializeImpl(QDataStream& d) const
{
    d << m_model
      << m_srcParentPath
      << m_dstParentPath;

    d << (qint32) m_srcRow;
    d << (qint32) m_dstRow;
    d << (qint32) m_count;
}

void
Move::deserializeImpl(QDataStream& d)
{
    d >> m_model
            >> m_srcParentPath
            >> m_dstParentPath;

    qint32 v;
    d >> v;
    m_srcRow = v;
    d >> v;
    m_dstRow = v;
    d >> v;
    m_count = v;
}
