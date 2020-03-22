// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateInspectorWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/Commands/State/AddStateProcess.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/Commands/TimeSync/SplitTimeSync.hpp>
#include <Scenario/DialogWidget/MessageTreeView.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/Separator.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/StyledButton.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QAbstractProxyModel>
#include <QHeaderView>
#include <QListView>
#include <QPushButton>
#include <QTableView>
#include <QWidget>

namespace Scenario
{
class MessageListProxy final : public QAbstractProxyModel
{

public:
  using QAbstractProxyModel::QAbstractProxyModel;
  MessageItemModel* source() const
  {
    return static_cast<MessageItemModel*>(sourceModel());
  }
  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override
  {
    if (parent == QModelIndex{})
    {
      if (row >= (int)rowCount({}) || row < 0)
        return {};

      if (column >= 2 || column < 0)
        return {};

      if (!source())
        return {};

      if (auto obj = getNthChild(source()->rootNode(), row))
        return createIndex(row, column, obj);
    }
    return {};
  }

  QModelIndex parent(const QModelIndex& child) const override { return {}; }

  QVariant data(const QModelIndex& proxyIndex, int role = Qt::DisplayRole)
      const override
  {
    auto ptr = proxyIndex.internalPointer();
    if (!ptr)
      return {};
    Process::MessageNode& msg = *static_cast<Process::MessageNode*>(ptr);

    if (proxyIndex.column() == 0)
    {
      if (role == Qt::DisplayRole)
      {
        return Process::address(msg).toString();
      }
    }
    else if (proxyIndex.column() == 1)
    {
      auto val = msg.value();
      if (val)
      {
        return valueColumnData(msg, role);
      }
    }
    return {};
  }
  int rowCount(const QModelIndex& parent) const override
  {
    if (parent == QModelIndex() && source())
    {
      return countNodes(source()->rootNode());
    }
    return 0;
  }
  int columnCount(const QModelIndex& parent) const override { return 2; }
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const override
  {
    auto idx = proxyIndex.internalPointer();
    if (!idx)
      return {};

    auto ptr = static_cast<Process::MessageNode*>(idx);
    auto parent = ptr->parent();
    if (!parent)
      return {};

    return createIndex(parent->indexOfChild(ptr), proxyIndex.column(), ptr);
  }
  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override
  {
    auto idx = sourceIndex.internalPointer();
    if (!idx)
      return {};

    auto ptr = static_cast<Process::MessageNode*>(idx);
    auto parent = ptr->parent();
    if (!parent)
      return {};

    if (!source())
      return {};

    auto row = getChildIndex(source()->rootNode(), ptr);
    return createIndex(row, sourceIndex.column(), idx);
  }

  QVariant
  headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if (orientation == Qt::Vertical)
      return {};

    if (role == Qt::DisplayRole)
      return (section == 0) ? tr("Address") : tr("Value");
    else
      return QAbstractProxyModel::headerData(section, orientation, role);
  }
};

StateInspectorWidget::StateInspectorWidget(
    const StateModel& object,
    const score::DocumentContext& doc,
    QWidget* parent)
    : Inspector::InspectorWidgetBase{object,
                                     doc,
                                     parent,
                                     tr("State (%1)")
                                         .arg(object.metadata().getName())}
    , m_model{object}
    , m_context{doc}
    , m_commandDispatcher{m_context.commandStack}
{
  setObjectName("StateInspectorWidget");
  setParent(parent);

  auto metadata = new MetadataWidget{
      m_model.metadata(), m_context.commandStack, &m_model, this};
  metadata->setupConnections(m_model);
  addHeader(metadata);

  std::vector<QWidget*> properties;
  auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
  SCORE_ASSERT(scenar);

  // State setup
  {
    auto splitEvent = new score::StyledButton;

    splitEvent->setObjectName("SplitCondition");
    splitEvent->setToolTip(tr("Split condition"));
    splitEvent->setStatusTip(tr("Split condition"));

    m_btnLayout.addWidget(splitEvent);
    connect(
        splitEvent,
        &QPushButton::clicked,
        this,
        &StateInspectorWidget::splitFromEvent);
    //properties.push_back(splitEvent);
  }

  {
    auto desynchronize = new score::StyledButton;

    desynchronize->setObjectName("Desynchronize");
    desynchronize->setToolTip(tr("Desynchronize"));
    desynchronize->setStatusTip(tr("Desynchronize"));
    m_btnLayout.addWidget(desynchronize);

    connect(
        desynchronize,
        &QPushButton::clicked,
        this,
        &StateInspectorWidget::splitFromNode);
   // properties.push_back(splitNode);
  }
  m_btnLayout.layout()->setSpacing(5);
  m_btnLayout.layout()->setContentsMargins(0,5,0,0);

  auto btns = new QWidget(this);
  btns->setLayout(m_btnLayout.layout());
  properties.push_back(btns);

  {
    auto tab = new QTabWidget;
    // list view

    auto lv = new QTableView{this};
    lv->verticalHeader()->hide();
    lv->horizontalHeader()->setCascadingSectionResizes(true);
    lv->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    lv->horizontalHeader()->setStretchLastSection(true);
    lv->setAlternatingRowColors(true);
    lv->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    lv->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    lv->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    lv->verticalHeader()->setDefaultSectionSize(14);

    auto proxy = new MessageListProxy{this};
    proxy->setSourceModel(&m_model.messages());
    lv->setModel(proxy);

    // tree view
    auto tv = new MessageTreeView{m_model, this};
    tv->header()->setCascadingSectionResizes(true);
    tv->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tv->header()->setStretchLastSection(true);
    tv->setAlternatingRowColors(true);
    tv->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tv->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tv->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    tab->addTab(tv, tr("Tree"));
    tab->addTab(lv, tr("List"));

    tab->setDocumentMode(true);

    properties.push_back(tab);
  }
  {
    auto lv = new QTableView;
    lv->verticalHeader()->hide();
    lv->horizontalHeader()->setCascadingSectionResizes(true);
    lv->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    lv->horizontalHeader()->setStretchLastSection(true);
    lv->setAlternatingRowColors(true);
    lv->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    lv->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    lv->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    lv->verticalHeader()->setDefaultSectionSize(14);
    lv->setModel(&this->m_model.controlMessages());
    properties.push_back(lv);
  }
  updateAreaLayout(properties);
}

void StateInspectorWidget::splitFromEvent()
{
  auto scenar = dynamic_cast<const Scenario::ProcessModel*>(m_model.parent());
  if (scenar)
  {
    auto& parentEvent = scenar->events.at(m_model.eventId());
    if (parentEvent.states().size() > 1)
    {
      auto cmd = new Scenario::Command::SplitEvent{
          *scenar, m_model.eventId(), {m_model.id()}};

      m_commandDispatcher.submit(cmd);
    }
  }
}

void StateInspectorWidget::splitFromNode()
{
  auto scenar = dynamic_cast<const Scenario::ProcessModel*>(m_model.parent());
  if (scenar)
  {
    auto& ev = Scenario::parentEvent(m_model, *scenar);
    auto& tn = Scenario::parentTimeSync(m_model, *scenar);
    if (ev.states().size() > 1)
    {
      MacroCommandDispatcher<Command::SplitStateMacro> disp{
          m_commandDispatcher.stack()};

      auto cmd = new Scenario::Command::SplitEvent{
          *scenar, m_model.eventId(), {m_model.id()}};
      disp.submit(cmd);
      auto cmd2 = new Scenario::Command::SplitTimeSync{tn, {cmd->newEvent()}};
      disp.submit(cmd2);
      disp.commit();
    }
    else if (ev.states().size() == 1)
    {
      if (tn.events().size() > 1)
      {
        auto cmd
            = new Scenario::Command::SplitTimeSync{tn, {m_model.eventId()}};
        m_commandDispatcher.submit(cmd);
      }
    }
  }
}
}
