#include "StateTreeView.hpp"

#include <DeviceExplorer/../Plugin/Widgets/MessageListEditor.hpp>
#include <State/StateItemModel.hpp>
#include <Commands/Event/State/AssignMessagesToState.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include "Document/State/StateModel.hpp"
#include <core/document/Document.hpp>
/*
StateTreeView::StateTreeView(const StateModel& model,
        DeviceExplorerModel* devexplorer,
        QWidget* parent):
    QTreeView{parent},
    m_model{const_cast<StateModel*>(&model)}, // sorry o lord for I have sinned
    m_devExplorer{devexplorer},
    m_dispatcher{iscore::IDocument::commandStack(model)}
{
    this->setModel(&m_model->states());
}

void StateTreeView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    QTreeView::mouseDoubleClickEvent(ev);
    auto sel = selectedIndexes();
    if(sel.empty())
        return;

    auto index = sel.first();

    auto node = index.isValid()
                ? static_cast<iscore::StateNode*>(index.internalPointer())
                : nullptr;

    if(node && node->is<iscore::MessageList>())
    {
        MessageListEditor ed(node->get<iscore::MessageList>(), m_devExplorer, this);
        int ret = ed.exec();

        if(ret)
        {
            auto cmd = new Scenario::Command::AssignMessagesToState{
                       iscore::IDocument::path(*m_model),
                       iscore::StatePath(*node),
                       ed.messages()};

            m_dispatcher.submitCommand(cmd);
        }
    }
    else
    {
        ISCORE_TODO;
    }

}

*/

#include <QAction>
MessageTreeView::MessageTreeView(
        const StateModel& model,
        DeviceExplorerModel* devexplorer,
        QWidget* parent):
    QTreeView{parent},
    m_model{const_cast<StateModel*>(&model)}, // TODO sorry o lord for I have sinned
    //m_devExplorer{devexplorer},
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

    this->setModel(&m_model->messages());

    m_removeNodesAction = new QAction(tr("Remove Nodes"), this);
    m_removeNodesAction->setShortcut(QKeySequence::Delete);
    m_removeNodesAction->setShortcutContext(Qt::WidgetShortcut);
    m_removeNodesAction->setEnabled(true);
    connect(m_removeNodesAction, &QAction::triggered,
            this, &MessageTreeView::removeNodes);
}

#include "Plugin/Commands/RemoveMessageNodes.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
iscore::MessageItemModel&MessageTreeView::model() const
{
    return *static_cast<iscore::MessageItemModel*>(QTreeView::model());
}

void MessageTreeView::removeNodes()
{
    qDebug(Q_FUNC_INFO);
    auto indexes = selectedIndexes();

    QList<iscore::Node*> nodes;
    for(auto index : indexes)
    {
        auto n = model().nodeFromModelIndex(index);
        if(!n->is<InvisibleRootNodeTag>())
            nodes.append(n);
    }

    auto cmd = new RemoveMessageNodes(
                      iscore::IDocument::path(model()),
                      iscore::filterUniqueParents(nodes));

    CommandDispatcher<> dispatcher{iscore::IDocument::commandStack(*m_model)};
    dispatcher.submitCommand(cmd);
}

#include <QContextMenuEvent>
#include <QMenu>
void MessageTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu contextMenu{this};

    contextMenu.addAction(m_removeNodesAction);
    contextMenu.exec(event->globalPos());
}

/*
void MessageTreeView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    QTreeView::mouseDoubleClickEvent(ev);
    auto sel = selectedIndexes();
    if(sel.empty())
        return;

    auto index = sel.first();

    auto node = index.isValid()
                ? static_cast<iscore::StateNode*>(index.internalPointer())
                : nullptr;

    if(node && node->is<iscore::AddressSettings>())
    {
        const auto& addr = node->get<iscore::AddressSettings>();
        MessageListEditor ed(node->get<iscore::MessageList>(), m_devExplorer, this);
        int ret = ed.exec();

        if(ret)
        {
            auto cmd = new Scenario::Command::AssignMessagesToState{
                       iscore::IDocument::path(*m_model),
                       iscore::StatePath(*node),
                       ed.messages()};

            m_dispatcher.submitCommand(cmd);
        }
    }
    else
    {
        ISCORE_TODO;
    }
}
*/
