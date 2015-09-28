#include "DeviceExplorerCommandCreator.hpp"

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <core/command/CommandStack.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include "Commands/Move.hpp"
#include "Commands/Insert.hpp"
#include "Commands/Cut.hpp"
#include "Commands/Paste.hpp"
#include "Commands/RemoveMessageNodes.hpp"


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

/*
  Copy|Cut / Paste behavior

  We do not do : serialize for Copy, or serialize+remove for Cut
  but we keep the nodes alive.
  We think that deleting nodes may be costly.

  We store cut nodes in m_cutNodes.

  We need to store several cut nodes to be able to do several undos
  on a CutCommands.
  (Merge CutCommands together would not be a solution as they can be interlaced with other commands).

  BUT IT MEANS that cut nodes are never deleted during the lifetime of the Model !

*/
QModelIndex DeviceExplorerCommandCreator::copy(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    iscore::Node* n = m_model->nodeFromModelIndex(index);

    iscore::Node* copiedNode = new iscore::Node(*n);
    const bool isDevice = n->is<iscore::DeviceSettings>();

    if(! m_model->m_cutNodes.isEmpty() && m_model->m_lastCutNodeIsCopied)
    {
        //necessary to avoid that several successive copies fill m_cutNodes (copy is not a command)
        iscore::Node* prevCopiedNode = m_model->m_cutNodes.pop().first;
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

    QString name = m_model->nodeFromModelIndex(index)->displayName();
    Cut* cmd = new Cut{
                iscore::NodePath{index.parent()},
                index.row(),
                tr("Cut %1").arg(name),
                *m_model};

    ISCORE_ASSERT(m_cmdQ);
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


    QString name = (index.isValid() ? m_model->nodeFromModelIndex(index)->displayName() : "");
    Paste* cmd = new Paste{
            iscore::NodePath{index.parent()},
            index.row(),
            tr("Paste %1").arg(name),
            *m_model};
    ISCORE_ASSERT(m_cmdQ);
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

    iscore::Node* n = m_model->nodeFromModelIndex(index);
    iscore::Node* parent = n->parent();
    ISCORE_ASSERT(parent);

    const int oldRow = index.row();
    const int newRow = oldRow - 1;

    iscore::NodePath parentPath{*parent};
    Move* cmd = new Move{
                parentPath,
                oldRow,
                1,
                parentPath,
                newRow,
                tr("Move up %1").arg(n->displayName()) ,
                *m_model};
    ISCORE_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }
    iscore::NodePath path{*n};
    path.back()--;

    return m_model->convertPathToIndex(path); // (newRow, 0, n);
}

QModelIndex DeviceExplorerCommandCreator::moveDown(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    iscore::Node* n = m_model->nodeFromModelIndex(index);
    iscore::Node* parent = n->parent();
    ISCORE_ASSERT(parent);

    int oldRow = index.row();
    ISCORE_ASSERT(parent->indexOfChild(n) == oldRow);
    int newRow = oldRow + 1;

    if(newRow >= parent->childCount())
    {
        return index;
    }

    iscore::NodePath parentPath{*parent};
    Move* cmd = new Move{
            parentPath,
            oldRow,
            1,
             parentPath,
            newRow + 1,
             tr("Move down %1").arg(n->displayName()) ,
            *m_model};
    //newRow+1 because moved before, cf doc.
    ISCORE_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }

    iscore::NodePath path{*n};
    path.back()++;  // path become the new path
    return m_model->convertPathToIndex(path);
}

QModelIndex DeviceExplorerCommandCreator::promote(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    iscore::Node* n = m_model->nodeFromModelIndex(index);
    iscore::Node* parent = n->parent();
    ISCORE_ASSERT(parent);

    if(parent == &m_model->rootNode())
    {
        return index;    // Already a top-level item
    }

    iscore::Node* grandParent = parent->parent();
    ISCORE_ASSERT(grandParent);

    if(grandParent == &m_model->rootNode())
    {
        return index;    //We cannot move an Address at Device level.
    }

    int row = parent->indexOfChild(n);
    int rowParent = grandParent->indexOfChild(parent);
    iscore::NodePath parentPath{*parent};
    Move* cmd = new Move{
                parentPath,
                row,
                1,
                iscore::NodePath{*grandParent},
                rowParent + 1,
                tr("Promote %1").arg(n->displayName()) ,
                *m_model};
    ISCORE_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }

    parentPath.removeLast();
    return m_model->convertPathToIndex(parentPath);
}

QModelIndex DeviceExplorerCommandCreator::demote(const QModelIndex &index)
{
    if(!index.isValid())
    {
        return index;
    }

    iscore::Node* n = m_model->nodeFromModelIndex(index);
    iscore::Node* parent = n->parent();
    ISCORE_ASSERT(parent);

    if(parent == &m_model->rootNode())
    {
        return index;    //we can not demote/moveRight device nodes
    }

    int row = parent->indexOfChild(n);

    if(row == 0)
    {
        return index;    // No preceding sibling to move this under
    }

    const auto& sibling = parent->childAt(row - 1);

    iscore::NodePath newPath{sibling};
    Move* cmd = new Move{
                iscore::NodePath{*parent},
                row,
                1,
                newPath ,
                sibling.childCount(),
                tr("Demote %1").arg(n->displayName()) ,
                *m_model};
    ISCORE_ASSERT(m_cmdQ);
    m_cmdQ->redoAndPush(cmd);

    if(! m_cachedResult)
    {
        delete m_cmdQ->command(0);
        m_cachedResult = true;
        return index;
    }

    newPath.append(sibling.childCount() - 1);

    return m_model->convertPathToIndex(newPath);
}

void DeviceExplorerCommandCreator::setCachedResult(DeviceExplorer::Result r)
{
    m_cachedResult = r;
}

