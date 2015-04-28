#include "DeviceExplorerCommandCreator.hpp"

#include <DeviceExplorer/Node/Node.hpp>

#include "Commands/Move.hpp"
#include "Commands/Insert.hpp"
#include "Commands/Cut.hpp"
#include "Commands/Paste.hpp"
#include "Commands/EditData.hpp"


using namespace DeviceExplorer::Command;

DeviceExplorerCommandCreator::DeviceExplorerCommandCreator(DeviceExplorerModel *model):
    m_cachedResult{true},
    m_model{model}
{

}

DeviceExplorerCommandCreator::~DeviceExplorerCommandCreator()
{

}

void DeviceExplorerCommandCreator::setCommandQueue(iscore::CommandStack *q)
{
    m_cmdQ = q;
}

QModelIndex DeviceExplorerCommandCreator::copy(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    Node* n = m_model->nodeFromModelIndex(index);
    Q_ASSERT(n);

    Node* copiedNode = n->clone();
    const bool isDevice = n->isDevice();

    if(! m_model->m_cutNodes.isEmpty() && m_model->m_lastCutNodeIsCopied)
    {
        //necessary to avoid that several successive copies fill m_cutNodes (copy is not a command)
        Node* prevCopiedNode = m_model->m_cutNodes.pop().first;
        delete prevCopiedNode;
    }

    m_model->m_cutNodes.push(CutElt(copiedNode, isDevice));
    m_model->m_lastCutNodeIsCopied = true;

    return index;
}

QModelIndex DeviceExplorerCommandCreator::cut(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    Cut* cmd = new Cut;

    QString name = (index.isValid() ? m_model->nodeFromModelIndex(index)->name() : "");
    cmd->set(Path{index.parent()}, index.row(),
             tr("Cut %1").arg(name), iscore::IDocument::path(m_model));

    Q_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return m_cachedResult.index;
    }

    return m_cachedResult.index;
}

QModelIndex DeviceExplorerCommandCreator::paste(const QModelIndex &index)
{
    if(m_model->m_cutNodes.isEmpty())
    {
        return index;
    }

    if(! index.isValid() && ! m_model->m_cutNodes.top().second)  //we can not pass addresses at top level
    {
        return index;
    }


    Paste* cmd = new Paste;

    QString name = (index.isValid() ? m_model->nodeFromModelIndex(index)->name() : "");
    cmd->set(Path{index.parent()}, index.row(),
             tr("Paste %1").arg(name), iscore::IDocument::path(m_model));
    Q_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return m_cachedResult.index;
    }

    return m_cachedResult.index;
}

QModelIndex DeviceExplorerCommandCreator::moveUp(const QModelIndex &index)
{
    if(!index.isValid() || index.row() <= 0)
    {
        return index;
    }

    Node* n = m_model->nodeFromModelIndex(index);
    Q_ASSERT(n);
    Node* parent = n->parent();
    Q_ASSERT(parent);

    const int oldRow = index.row();
    const int newRow = oldRow - 1;

    Node* grandparent = parent->parent();
    Q_ASSERT(grandparent);

    Move* cmd = new Move;
    cmd->set(Path(parent), oldRow, 1,
             Path(parent), newRow,
             tr("Move up %1").arg(n->name()) , iscore::IDocument::path(m_model));
    Q_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }
    Path path{n};
    path.back()--;

    return m_model->pathToIndex(path); // (newRow, 0, n);
}

QModelIndex DeviceExplorerCommandCreator::moveDown(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    Node* n = m_model->nodeFromModelIndex(index);
    Q_ASSERT(n);
    Node* parent = n->parent();
    Q_ASSERT(parent);

    int oldRow = index.row();
    Q_ASSERT(parent->indexOfChild(n) == oldRow);
    int newRow = oldRow + 1;

    if(newRow >= parent->childCount())
    {
        return index;
    }

    Node* grandparent = parent->parent();
    Q_ASSERT(grandparent);

    Move* cmd = new Move;
    cmd->set(Path(parent), oldRow, 1,
             Path(parent), newRow + 1,
             tr("Move down %1").arg(n->name()) , iscore::IDocument::path(m_model));
    //newRow+1 because moved before, cf doc.
    Q_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }

    Path path{n};
    path.back()++;  // path become the new path
    return m_model->pathToIndex(path);
}

QModelIndex DeviceExplorerCommandCreator::promote(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    Node* n = m_model->nodeFromModelIndex(index);
    Q_ASSERT(n);
    Node* parent = n->parent();
    Q_ASSERT(parent);

    if(parent == m_model->rootNode())
    {
        return index;    // Already a top-level item
    }

    Node* grandParent = parent->parent();
    Q_ASSERT(grandParent);

    if(grandParent == m_model->rootNode())
    {
        return index;    //We cannot move an Address at Device level.
    }

    int row = parent->indexOfChild(n);
    int rowParent = grandParent->indexOfChild(parent);

    Move* cmd = new Move;
    Path parentPath = Path(parent);
    cmd->set(parentPath, row, 1,
             Path(grandParent), rowParent + 1,
             tr("Promote %1").arg(n->name()) , iscore::IDocument::path(m_model));
    Q_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }

    parentPath.removeLast();
    return m_model->pathToIndex(parentPath);
}

QModelIndex DeviceExplorerCommandCreator::demote(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    Node* n = m_model->nodeFromModelIndex(index);
    Q_ASSERT(n);
    Node* parent = n->parent();
    Q_ASSERT(parent);

    if(parent == m_model->rootNode())
    {
        return index;    //we can not demote/moveRight device nodes
    }

    int row = parent->indexOfChild(n);

    if(row == 0)
    {
        return index;    // No preceding sibling to move this under
    }

    Node* sibling = parent->childAt(row - 1);
    Q_ASSERT(sibling);

    Move* cmd = new Move;
    Path newPath{sibling};
    cmd->set(Path(parent), row, 1,
             newPath , sibling->childCount(),
             tr("Demote %1").arg(n->name()) , iscore::IDocument::path(m_model));
    Q_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }

    newPath.append(sibling->childCount()-1);

    return m_model->pathToIndex(newPath);
}

void DeviceExplorerCommandCreator::setCachedResult(DeviceExplorerModel::Result r)
{
    m_cachedResult = r;
}

