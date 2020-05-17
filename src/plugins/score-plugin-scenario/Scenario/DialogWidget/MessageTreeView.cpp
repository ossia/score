// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageTreeView.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/State/RemoveMessageNodes.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/tools/Bind.hpp>

#include <QAbstractItemView>
#include <QAction>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QResizeEvent>
#include <qnamespace.h>

class QWidget;
namespace Scenario
{
MessageTreeView::MessageTreeView(const StateModel& model, QWidget* parent)
    : QTreeView{parent}
    , m_model{model}
    , m_dispatcher{score::IDocument::documentContext(model).commandStack}
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
  m_removeNodesAction->setShortcut(Qt::Key_Backspace);
  m_removeNodesAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_removeNodesAction->setEnabled(true);
  connect(m_removeNodesAction, &QAction::triggered, this, &MessageTreeView::removeNodes);
  addAction(m_removeNodesAction);

  expandAll();
  con(m_model.messages(), &MessageItemModel::modelReset, this, &QTreeView::expandAll);

  header()->resizeSection(
      (int)MessageItemModel::Column::Name, (1 - m_valueColumnSize - 0.1) * this->width());
  header()->resizeSection((int)MessageItemModel::Column::Value, m_valueColumnSize * this->width());

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

  auto cmd = new Command::RemoveMessageNodes(m_model, filterUniqueParents(nodes));

  CommandDispatcher<> dispatcher{score::IDocument::documentContext(m_model).commandStack};
  dispatcher.submit(cmd);
}

void MessageTreeView::resizeEvent(QResizeEvent* ev)
{
  ev->ignore();
  header()->resizeSection(
      (int)MessageItemModel::Column::Name, (1 - m_valueColumnSize - 0.1) * this->width());
  header()->resizeSection((int)MessageItemModel::Column::Value, m_valueColumnSize * this->width());
}

void MessageTreeView::contextMenuEvent(QContextMenuEvent* event)
{
  QMenu contextMenu{this};

  contextMenu.addAction(m_removeNodesAction);
  contextMenu.exec(event->globalPos());
}
}
