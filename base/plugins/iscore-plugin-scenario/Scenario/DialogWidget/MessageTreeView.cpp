#include <QAbstractItemView>
#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFlags>
#include <QHeaderView>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QResizeEvent>
#include <Scenario/Commands/State/RemoveMessageNodes.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <qnamespace.h>

#include <QSet>
#include <QString>

#include "MessageTreeView.hpp"
#include <Device/Node/DeviceNode.hpp>
#include <Process/State/MessageNode.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/model/tree/TreeNode.hpp>

class QWidget;
namespace Scenario
{
MessageTreeView::MessageTreeView(const StateModel& model, QWidget* parent)
    : QTreeView{parent}
    , m_model{model}
    , m_dispatcher{iscore::IDocument::documentContext(model).commandStack}
{
  setAllColumnsShowFocus(true);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setEditTriggers(
      QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);

  setDragEnabled(true);
  setAcceptDrops(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::CopyAction);
  setDragDropOverwriteMode(false);
  setUniformRowHeights(true);

  this->setModel(&m_model.messages());

  m_removeNodesAction = new QAction(tr("Remove Nodes"), this);
  m_removeNodesAction->setShortcut(Qt::Key_Backspace);
  m_removeNodesAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_removeNodesAction->setEnabled(true);
  connect(
      m_removeNodesAction, &QAction::triggered, this,
      &MessageTreeView::removeNodes);
  addAction(m_removeNodesAction);

  expandAll();
  con(m_model.messages(), &MessageItemModel::modelReset, this,
      &QTreeView::expandAll);

  header()->resizeSection(
      (int)MessageItemModel::Column::Name,
      (1 - m_valueColumnSize - 0.1) * this->width());
  header()->resizeSection(
      (int)MessageItemModel::Column::Value, m_valueColumnSize * this->width());

  this->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
}

MessageItemModel& MessageTreeView::model() const
{
  return *static_cast<MessageItemModel*>(QTreeView::model());
}

void MessageTreeView::removeNodes()
{
  auto indexes = selectedIndexes();

  std::vector<const Process::MessageNode*> nodes;
  for (auto index : indexes)
  {
    auto& n = model().nodeFromModelIndex(index);
    if (n.parent())
      nodes.push_back(&n);
  }

  auto cmd = new Command::RemoveMessageNodes(
      model().stateModel, Device::filterUniqueParents(nodes));

  CommandDispatcher<> dispatcher{
      iscore::IDocument::documentContext(m_model).commandStack};
  dispatcher.submitCommand(cmd);
}

void MessageTreeView::resizeEvent(QResizeEvent* ev)
{
  ev->ignore();
  header()->resizeSection(
      (int)MessageItemModel::Column::Name,
      (1 - m_valueColumnSize - 0.1) * this->width());
  header()->resizeSection(
      (int)MessageItemModel::Column::Value, m_valueColumnSize * this->width());
}

void MessageTreeView::contextMenuEvent(QContextMenuEvent* event)
{
  QMenu contextMenu{this};

  contextMenu.addAction(m_removeNodesAction);
  contextMenu.exec(event->globalPos());
}
}
