#include <Scenario/Commands/State/RemoveMessageNodes.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QAbstractItemView>
#include <QAction>
#include <QEvent>
#include <QFlags>
#include <QHeaderView>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <qnamespace.h>

#include <QSet>
#include <QString>

#include <Device/Node/DeviceNode.hpp>
#include "MessageTreeView.hpp"
#include <Process/State/MessageNode.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/TreeNode.hpp>

class QWidget;

MessageTreeView::MessageTreeView(
        const StateModel& model,
        QWidget* parent):
    QTreeView{parent},
    m_model{model},
    m_dispatcher{iscore::IDocument::documentContext(model).commandStack}
{
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);

    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::CopyAction);
    setDragDropOverwriteMode(false);
    setUniformRowHeights(true);

    this->setModel(&m_model.messages());

    m_removeNodesAction = new QAction(tr("Remove Nodes"), this);
    m_removeNodesAction->setShortcut(QKeySequence::Delete);
    m_removeNodesAction->setShortcutContext(Qt::WidgetShortcut);
    m_removeNodesAction->setEnabled(true);
    connect(m_removeNodesAction, &QAction::triggered,
            this, &MessageTreeView::removeNodes);

    expandAll();
    con(m_model.messages(), &MessageItemModel::modelReset,
        this, &QTreeView::expandAll);

    header()->resizeSection((int)MessageItemModel::Column::Name, (1-m_valueColumnSize - 0.1)*this->width());
    header()->resizeSection((int)MessageItemModel::Column::Value, m_valueColumnSize*this->width());
}

MessageItemModel& MessageTreeView::model() const
{
    return *static_cast<MessageItemModel*>(QTreeView::model());
}

void MessageTreeView::removeNodes()
{
    auto indexes = selectedIndexes();

    QList<const MessageNode*> nodes;
    for(auto index : indexes)
    {
        auto& n = model().nodeFromModelIndex(index);
        if(n.parent())
            nodes.append(&n);
    }

    auto cmd = new RemoveMessageNodes(
                      model(),
                      Device::filterUniqueParents(nodes));

    CommandDispatcher<> dispatcher{iscore::IDocument::documentContext(m_model).commandStack};
    dispatcher.submitCommand(cmd);
}

void MessageTreeView::resizeEvent(QResizeEvent* ev)
{
    ev->ignore();
    header()->resizeSection((int)MessageItemModel::Column::Name, (1-m_valueColumnSize - 0.1)*this->width());
    header()->resizeSection((int)MessageItemModel::Column::Value, m_valueColumnSize*this->width());
}

void MessageTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu contextMenu{this};

    contextMenu.addAction(m_removeNodesAction);
    contextMenu.exec(event->globalPos());
}

