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
#include <score/widgets/SelectionButton.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

#include <Scenario/Commands/Cohesion/RefreshStates.hpp>
#include <Scenario/Commands/Cohesion/SnapshotParameters.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/Separator.hpp>
#include <score/widgets/SetIcons.hpp>
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
  MessageItemModel* source() const { return static_cast<MessageItemModel*>(sourceModel()); }
  QModelIndex index(int row, int column, const QModelIndex& parent) const override
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

  QVariant data(const QModelIndex& proxyIndex, int role = Qt::DisplayRole) const override
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

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
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
    : Inspector::
        InspectorWidgetBase{object, doc, parent, tr("State (%1)").arg(object.metadata().getName())}
    , m_model{object}
    , m_context{doc}
    , m_commandDispatcher{m_context.commandStack}
{
  setObjectName("StateInspectorWidget");
  setParent(parent);

  auto metadata = new MetadataWidget{m_model.metadata(), m_context.commandStack, &m_model, this};
  metadata->setupConnections(m_model);
  addHeader(metadata);

  std::vector<QWidget*> properties;
  auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
  SCORE_ASSERT(scenar);

  // State setup
  {
    auto splitEvent = new QToolButton;
    splitEvent->setIcon(makeIcons(
        QStringLiteral(":/icons/split_condition_on.png"),
        QStringLiteral(":/icons/split_condition_hover.png"),
        QStringLiteral(":/icons/split_condition_off.png"),
        QStringLiteral(":/icons/split_condition_off.png")));
    splitEvent->setToolTip(tr("Split condition"));
    splitEvent->setStatusTip(tr("Split condition"));

    splitEvent->setAutoRaise(true);
    splitEvent->setIconSize(QSize{28,28});
    m_btnLayout.addWidget(splitEvent);
    connect(splitEvent, &QPushButton::clicked, this, &StateInspectorWidget::splitFromEvent);
  }

  {
    auto desynchronize = new QToolButton;
    desynchronize->setIcon(makeIcons(
        QStringLiteral(":/icons/desynchronize_on.png"),
        QStringLiteral(":/icons/desynchronize_hover.png"),
        QStringLiteral(":/icons/desynchronize_off.png"),
        QStringLiteral(":/icons/desynchronize_off.png")));
    desynchronize->setToolTip(tr("Desynchronize"));
    desynchronize->setStatusTip(tr("Desynchronize"));

    desynchronize->setAutoRaise(true);
    desynchronize->setIconSize(QSize{28,28});

    m_btnLayout.addWidget(desynchronize);

    connect(desynchronize, &QPushButton::clicked, this, &StateInspectorWidget::splitFromNode);
  }
  {
    auto snapshot = new QToolButton;
    snapshot->setShortcut(tr("Ctrl+L"));
    snapshot->setToolTip(tr("Ctrl+L"));
    snapshot->setIcon(makeIcons(
        QStringLiteral(":/icons/snapshot_on.png"),
        QStringLiteral(":/icons/snapshot_hover.png"),
        QStringLiteral(":/icons/snapshot_off.png"),
        QStringLiteral(":/icons/snapshot_disabled.png")));
    snapshot->setIconSize(QSize{28,28});
    snapshot->setAutoRaise(true);

    connect(snapshot, &QToolButton::clicked, this, [this] {
        SnapshotParametersInStates(this->context());
    });
    m_btnLayout.addWidget(snapshot);
  }
  {
    auto refresh = new QToolButton;
    refresh->setShortcut(tr("Ctrl+U"));
    refresh->setToolTip(tr("Ctrl+U"));
    refresh->setIcon(makeIcons(
                        QStringLiteral(":/icons/refresh_on.png"),
                       QStringLiteral(":/icons/refresh_hover.png"),
                        QStringLiteral(":/icons/refresh_off.png"),
                        QStringLiteral(":/icons/refresh_disabled.png")));
    refresh->setIconSize(QSize{28,28});
    refresh->setAutoRaise(true);

    connect(refresh, &QToolButton::clicked, this, [this] {
      Scenario::Command::RefreshStates(this->context());
    });
    m_btnLayout.addWidget(refresh);
  }
  {
    auto trigger = new QToolButton;
    trigger->setCheckable(true);
    trigger->setChecked(Scenario::parentTimeSync(m_model, *scenar).active());
    //trigger->setShortcut(tr("Ctrl+U"));
    //trigger->setToolTip(tr("Ctrl+U"));
    trigger->setIcon(makeIcons(
                        QStringLiteral(":/icons/trigger_on.png"),
                       QStringLiteral(":/icons/trigger_hover.png"),
                        QStringLiteral(":/icons/trigger_off.png"),
                       QStringLiteral(":/icons/trigger_disabled.png")));
    trigger->setAutoRaise(true);
    trigger->setIconSize(QSize{28,28});

    connect(trigger, &QToolButton::toggled, this, [this] (bool b) {
      if(b)
      {
        auto& addTrig = context().app.actions.action<Actions::AddTrigger>();
        addTrig.action()->trigger();
      }
      else
      {
        auto& rmTrig = context().app.actions.action<Actions::RemoveTrigger>();
        rmTrig.action()->trigger();
      }
    });
    m_btnLayout.addWidget(trigger);
  }
  {
    auto condition = new QToolButton;
    condition->setCheckable(true);
    condition->setChecked(Scenario::parentTimeSync(m_model, *scenar).active());
    //trigger->setShortcut(tr("Ctrl+U"));
    //trigger->setToolTip(tr("Ctrl+U"));
    condition->setIcon(makeIcons(
                        QStringLiteral(":/icons/condition_on.png"),
                         QStringLiteral(":/icons/condition_hover.png"),
                        QStringLiteral(":/icons/condition_off.png"),
                       QStringLiteral(":/icons/condition_disabled.png")));
    condition->setIconSize(QSize{28,28});
    condition->setAutoRaise(true);

    connect(condition, &QToolButton::toggled, this, [this] (bool b) {
      if(b)
      {
        auto& addTrig = context().app.actions.action<Actions::AddCondition>();
        addTrig.action()->trigger();
      }
      else
      {
        auto& rmTrig = context().app.actions.action<Actions::RemoveCondition>();
        rmTrig.action()->trigger();
      }
    });
    m_btnLayout.addWidget(condition);
  }
  /* TODO play state ?
  {
    auto play = new QToolButton;
    play->setShortcut(tr("Ctrl+P"));
    play->setToolTip(tr("Ctrl+P"));
    play->setIcon(makeIcons(
        QStringLiteral(":/icons/play_on.png"),
        QStringLiteral(":/icons/play_off.png"),
        QStringLiteral(":/icons/play_disabled.png")));
    connect(play, &QToolButton::clicked, this, [this] {
      {
        auto ossia_state = Engine::score_to_ossia::state(*state, r_ctx);
        ossia_state.launch();
      }
    }
    });
    m_btnLayout.addWidget(snapshot);
  }
  */
  {
    QWidget* spacerWidget = new QWidget(this);
    spacerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacerWidget->setVisible(true);
    m_btnLayout.addWidget(spacerWidget);
  }

  m_btnLayout.setContentsMargins(0, 0, 0, 0);

  //auto btns = new QWidget(this);
  //btns->setLayout(&m_btnLayout);
  ((QBoxLayout*)metadata->layout())->insertLayout(0, &m_btnLayout);
  //properties.push_back(btns);
  //{
  //  auto frame = new QFrame;
  //  frame->setFrameShape(QFrame::HLine);
  //  frame->setFrameShadow(QFrame::Sunken);
  //  properties.push_back(frame);
  //}

  {
    auto tab = new QTabWidget;
    // list view

    auto lv = new QTableView{this};
    lv->verticalHeader()->hide();
    lv->horizontalHeader()->setCascadingSectionResizes(true);
    lv->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
    lv->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
      auto cmd = new Scenario::Command::SplitEvent{*scenar, m_model.eventId(), {m_model.id()}};

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
      MacroCommandDispatcher<Command::SplitStateMacro> disp{m_commandDispatcher.stack()};

      auto cmd = new Scenario::Command::SplitEvent{*scenar, m_model.eventId(), {m_model.id()}};
      disp.submit(cmd);
      auto cmd2 = new Scenario::Command::SplitTimeSync{tn, {cmd->newEvent()}};
      disp.submit(cmd2);
      disp.commit();
    }
    else if (ev.states().size() == 1)
    {
      if (tn.events().size() > 1)
      {
        auto cmd = new Scenario::Command::SplitTimeSync{tn, {m_model.eventId()}};
        m_commandDispatcher.submit(cmd);
      }
    }
  }
}
}
