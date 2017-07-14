// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/StateProcess.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QtAlgorithms>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/Commands/State/AddStateProcess.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/DialogWidget/MessageTreeView.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/Separator.hpp>
#include <iscore/widgets/TextLabel.hpp>

#include <QMenu>
#include <algorithm>

#include "StateInspectorWidget.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <QSizePolicy>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
namespace Scenario
{
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
      auto tv = new MessageTreeView{m_model, this};
      m_properties.push_back(tv);
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

void StateInspectorWidget::splitEvent()
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
      = m_context.app.interfaces<Process::
                         StateProcessInspectorWidgetDelegateFactoryList>();
  if (auto widg = fact.make(
          &Process::StateProcessInspectorWidgetDelegateFactory::make, process,
          m_context, sectionWidg))
  {
    sectionWidg->addContent(widg);
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
