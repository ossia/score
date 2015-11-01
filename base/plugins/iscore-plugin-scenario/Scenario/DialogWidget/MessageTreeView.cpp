#include "MessageTreeView.hpp"

#include <Scenario/Document/State/StateModel.hpp>
#include <DeviceExplorer/../Plugin/Widgets/MessageListEditor.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <core/document/Document.hpp>

#include <Scenario/Commands/State/RemoveMessageNodes.hpp>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <QAction>
MessageTreeView::MessageTreeView(
        const StateModel& model,
        QWidget* parent):
    QTreeView{parent},
    m_model{const_cast<StateModel*>(&model)}, // TODO sorry o lord for I have sinned
    m_dispatcher{iscore::IDocument::commandStack(model)}
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

    this->setModel(&m_model->messages());

    m_removeNodesAction = new QAction(tr("Remove Nodes"), this);
    m_removeNodesAction->setShortcut(QKeySequence::Delete);
    m_removeNodesAction->setShortcutContext(Qt::WidgetShortcut);
    m_removeNodesAction->setEnabled(true);
    connect(m_removeNodesAction, &QAction::triggered,
            this, &MessageTreeView::removeNodes);

    expandAll();
    con(m_model->messages(), &MessageItemModel::modelReset,
        this, &QTreeView::expandAll);

    header()->resizeSection((int)MessageItemModel::Column::Name, 300);
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
                      iscore::filterUniqueParents(nodes));

    CommandDispatcher<> dispatcher{iscore::IDocument::commandStack(*m_model)};
    dispatcher.submitCommand(cmd);
}

void MessageTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu contextMenu{this};

    contextMenu.addAction(m_removeNodesAction);
    contextMenu.exec(event->globalPos());
}

