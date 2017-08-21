// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateInspectorWidget.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/StateProcess.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/Commands/State/AddStateProcess.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/DialogWidget/MessageTreeView.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Commands/TimeNode/SplitTimeNode.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/Separator.hpp>
#include <iscore/widgets/TextLabel.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <QAbstractProxyModel>
#include <QTableView>
#include <QMenu>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QSizePolicy>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QtAlgorithms>
#include <QApplication>
#include <QTimer>
#include <algorithm>
namespace Scenario
{
class MessageListProxy final : public QAbstractProxyModel
{

public:
  MessageItemModel& source() const { return static_cast<MessageItemModel&>(*sourceModel()); }
  QModelIndex index(int row, int column, const QModelIndex& parent) const override
  {
    if (row >= (int)rowCount({}) || row < 0)
      return {};

    if (column >= 2 || column < 0)
      return {};

    if(auto obj = getNthChild(source().rootNode(), row))
      return createIndex(row, column, obj);

    return {};
  }

  QModelIndex parent(const QModelIndex& child) const override
  {
    return {};
  }

  QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const override
  {
    auto ptr = proxyIndex.internalPointer();
    if(!ptr)
      return {};
    Process::MessageNode& msg = *static_cast<Process::MessageNode*>(ptr);

    if(proxyIndex.column() == 0)
    {
      if(role == Qt::DisplayRole)
      {
        return Process::address(msg).toString();
      }
    }
    else if(proxyIndex.column() == 1)
    {
      auto val = msg.value();
      if(val)
      {
        return valueColumnData(msg, role);
      }
    }
    return {};
  }
  int rowCount(const QModelIndex& parent) const override
  {
    if(parent == QModelIndex())
    {
      return countNodes(source().rootNode());
    }
    return 0;
  }
  int columnCount(const QModelIndex& parent) const override
  {
    return 2;
  }
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const override
  {
    return {};
  }
  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override
  {
    return {};
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Vertical)
      return {};

    if(role == Qt::DisplayRole)
      return (section == 0) ? tr("Address") : tr("Value");
    else
      return QAbstractProxyModel::headerData(section, orientation, role);
  }
};

StateInspectorWidget::StateInspectorWidget(
    const StateModel& object,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : Inspector::InspectorWidgetBase{object, doc, parent}
    , m_model{object}
    , m_context{doc}
    , m_commandDispatcher{m_context.commandStack}
    , m_selectionDispatcher{m_context.selectionStack}
{
  setObjectName("StateInspectorWidget");
  setParent(parent);

  updateDisplayedValues();
  m_model.stateProcesses.added
      .connect<StateInspectorWidget, &StateInspectorWidget::on_stateProcessCreated>(
          this);
  m_model.stateProcesses.removed
      .connect<StateInspectorWidget, &StateInspectorWidget::on_stateProcessRemoved>(
          this);
}

void StateInspectorWidget::updateDisplayedValues()
{
  // Cleanup
  // OPTIMIZEME
  m_properties.clear();
  auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
  ISCORE_ASSERT(scenar);

  // State setup
  auto metadata = new MetadataWidget{
          m_model.metadata(), m_context.commandStack, &m_model, this};
  metadata->setupConnections(m_model);
  m_properties.push_back(metadata);
  m_properties.push_back(new iscore::HSeparator{this});

  {
      auto linkWidget = new QWidget;
      auto linkLay = new iscore::MarginLess<QHBoxLayout>{linkWidget};

      // Constraints setup
      if (m_model.previousConstraint())
      {
          auto btn = SelectionButton::make(
                      tr("Prev. Constraint"),
                      &scenar->constraint(*m_model.previousConstraint()),
                      m_selectionDispatcher,
                      this);

          linkLay->addWidget(btn);
      }
      if (m_model.nextConstraint())
      {
          auto btn = SelectionButton::make(
                      tr("Next Constraint"),
                      &scenar->constraint(*m_model.nextConstraint()),
                      m_selectionDispatcher,
                      this);

          linkLay->addWidget(btn);
      }
      linkLay->addStretch(1);

      m_properties.push_back(linkWidget);
  }

  {
      auto splitEvent = new QPushButton{tr("Put in new Event"), this};
      connect(splitEvent, &QPushButton::clicked,
              this, &StateInspectorWidget::splitFromEvent);
      m_properties.push_back(splitEvent);
  }

  {
      auto splitNode = new QPushButton{tr("Desynchronize"), this};
      connect(splitNode, &QPushButton::clicked,
              this, &StateInspectorWidget::splitFromNode);
      m_properties.push_back(splitNode);
  }
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

      auto proxy = new MessageListProxy{};
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

      m_properties.push_back(tab);
  }

  // State processes
  auto procWidg = new QWidget;
  auto procLay = new iscore::MarginLess<QVBoxLayout>;
  {
    auto addProcButton = new QPushButton;
    addProcButton->setText(QStringLiteral("Add process"));
    procLay->addWidget(addProcButton);

    // add new process dialog
    delete m_addProcess;
    m_addProcess = new AddStateProcessDialog{
        m_context.app.interfaces<Process::StateProcessList>(), this};

    // CONNECTIONS
    connect(
        addProcButton, &QPushButton::pressed, m_addProcess,
        &AddStateProcessDialog::launchWindow);

    connect(
        m_addProcess, &AddStateProcessDialog::okPressed, this,
        &StateInspectorWidget::createStateProcess);

    for (auto& proc : m_model.stateProcesses)
    {
      procLay->addWidget(displayStateProcess(proc));
    }

    procWidg->setLayout(procLay);
  }

  m_properties.push_back(procWidg);
  updateAreaLayout(m_properties);
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

      m_commandDispatcher.submitCommand(cmd);
    }
  }
}

void StateInspectorWidget::splitFromNode()
{
    auto scenar = dynamic_cast<const Scenario::ProcessModel*>(m_model.parent());
    if (scenar)
    {
        auto& ev = Scenario::parentEvent(m_model, *scenar);
        auto& tn = Scenario::parentTimeNode(m_model, *scenar);
        if (ev.states().size() > 1)
        {
            MacroCommandDispatcher<Command::SplitStateMacro> disp{m_commandDispatcher.stack()};

            auto cmd = new Scenario::Command::SplitEvent{
                    *scenar, m_model.eventId(), {m_model.id()}
                };
            disp.submitCommand(cmd);
            auto cmd2 = new Scenario::Command::SplitTimeNode{
                    tn, {cmd->newEvent()}
                };
            disp.submitCommand(cmd2);
            disp.commit();
        }
        else if(ev.states().size() == 1)
        {
            if(tn.events().size() > 1)
            {
                auto cmd = new Scenario::Command::SplitTimeNode{
                        tn, {m_model.eventId()}
                };
                m_commandDispatcher.submitCommand(cmd);
            }
        }
    }
}

void StateInspectorWidget::on_stateProcessCreated(const Process::StateProcess&)
{
  updateDisplayedValues();
}

void StateInspectorWidget::on_stateProcessRemoved(const Process::StateProcess&)
{
  updateDisplayedValues();
}

void StateInspectorWidget::createStateProcess(
    const UuidKey<Process::StateProcessFactory>& key)
{
  auto cmd = new Command::AddStateProcessToState(m_model, key);
  m_commandDispatcher.submitCommand(cmd);
}

Inspector::InspectorSectionWidget*
StateInspectorWidget::displayStateProcess(const Process::StateProcess& process)
{
  using namespace iscore;

  // New Section
  auto sectionWidg
      = new Inspector::InspectorSectionWidget(process.prettyName(), true);
  sectionWidg->showMenu(true);

  const auto& fact
      = m_context.app.interfaces<Inspector::InspectorWidgetList>();
  auto widgs = fact.make(m_context, {&process}, sectionWidg);
  if (!widgs.empty())
  {
    sectionWidg->addContent(widgs[0]);
  }

  // delete process
  ISCORE_TODO_("Delete state process");

  auto delAct = sectionWidg->menu()->addAction(tr("Remove State Process"));
  connect(
      delAct, &QAction::triggered, this, [ =, id = process.id() ]() {
        auto cmd = new Command::RemoveStateProcess{
            m_model, id};
        emit m_commandDispatcher.submitCommand(cmd);
      },
      Qt::QueuedConnection);

  return sectionWidg;
}
}
