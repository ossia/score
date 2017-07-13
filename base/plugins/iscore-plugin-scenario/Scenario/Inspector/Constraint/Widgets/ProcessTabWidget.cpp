// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessTabWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetList.hpp>

#include <iscore/document/DocumentContext.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <iscore/widgets/TextLabel.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include <Scenario/DialogWidget/AddProcessDialog.hpp>

#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/RemoveProcessFromConstraint.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <Process/Process.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>

#include <Scenario/Inspector/Constraint/Widgets/ProcessWidgetArea.hpp>

#include <QAction>
#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>

#include <boost/container/flat_map.hpp>
namespace Scenario
{

ProcessTabWidget::ProcessTabWidget(
    const ConstraintInspectorWidget& parentCstr, QWidget* parent)
    : QWidget(parent), m_constraintWidget{parentCstr}
{
  // CREATION

  // main layout
  auto processesLay = new iscore::MarginLess<QVBoxLayout>{this};

  // add new process widget
  auto addProc = new QWidget(this);
  auto addProcLayout = new iscore::MarginLess<QHBoxLayout>{addProc};

  auto addProcButton = new QToolButton;
  addProcButton->setText("+");
  QIcon addIcon = makeIcons(
      ":/icons/condition_add_on.png", ":/icons/condition_add_off.png");
  addProcButton->setIcon(addIcon);
  addProcButton->setAutoRaise(true);

  auto addProcText = new TextLabel(tr("Add Process"));
  addProcText->setStyleSheet(QStringLiteral("text-align : left;"));

  // add new process dialog
  m_addProcess = new AddProcessDialog{m_constraintWidget.processList(), this};

  // CONNECTIONS
  connect(
      addProcButton, &QToolButton::pressed, m_addProcess,
      &AddProcessDialog::launchWindow);

  connect(
      m_addProcess, &AddProcessDialog::okPressed, this,
      &ProcessTabWidget::createProcess);

  // LAYOUTS
  addProcLayout->addWidget(addProcButton);
  addProcLayout->addWidget(addProcText);

  processesLay->addWidget(addProc);
  processesLay->addStretch(1);
}

void ProcessTabWidget::createProcess(
    const UuidKey<Process::ProcessModel>& processName)
{
  auto cmd = Command::make_AddProcessToConstraint(
      m_constraintWidget.model(), processName);
  m_constraintWidget.commandDispatcher()->submitCommand(cmd);
}

void ProcessTabWidget::displayProcess(
    const Process::ProcessModel& process,
    bool expanded)
{
  using namespace iscore;

  // New Section
  auto newProc = new ProcessWidgetArea(
      process, *m_constraintWidget.commandDispatcher(),
      process.metadata().getName(), true);

  // name changing connections
  connect(newProc, &ProcessWidgetArea::nameChanged, this, [&](QString s) {
    ask_processNameChanged(process, s);
  });
  con(process.metadata(), &iscore::ModelMetadata::NameChanged, newProc,
      &ProcessWidgetArea::renameSection);

  // ***********************
  // PROCESS

  // add view in new slot
  newProc->showMenu(true);

  const auto& fact
      = m_constraintWidget.context()
            .app.interfaces<Process::InspectorWidgetDelegateFactoryList>();
  if (auto widg = fact.make(
          &Process::InspectorWidgetDelegateFactory::make, process,
          m_constraintWidget.context(), newProc))
  {
    newProc->addContent(widg);

    newProc->menu()->addAction(
        process.objectName() + " " + QString::number(process.id_val()));
    auto act = newProc->menu()->addAction(tr("Display in new slot"));

    connect(act, &QAction::triggered, this, [&]() {
      createLayerInNewSlot(process.id());
    });
  }

  // delete process
  auto delAct = newProc->menu()->addAction(tr("Remove Process"));
  delAct->setIcon(genIconFromPixmaps(
      QStringLiteral(":/icons/delete_on.png"), QStringLiteral(":/icons/delete_off.png")));
  connect(
      delAct, &QAction::triggered, this, [ =, id = process.id() ]() {
        auto cmd = new Command::RemoveProcessFromConstraint{
            m_constraintWidget.model(), id};
        emit m_constraintWidget.commandDispatcher()->submitCommand(cmd);
      },
      Qt::QueuedConnection);

  // Start & end state
  auto stateWidget = new QWidget;
  auto stateLayout = new iscore::MarginLess<QFormLayout>{stateWidget};

  if (auto start = process.startStateData())
  {
    auto startWidg = m_constraintWidget.widgetList()
                         .make(m_constraintWidget.context(), {start}, newProc)
                         .first();

    if (startWidg)
      stateLayout->addRow(tr("Start "), startWidg);
  }

  if (auto end = process.endStateData())
  {
    auto endWidg = m_constraintWidget.widgetList()
                       .make(m_constraintWidget.context(), {end}, newProc)
                       .first();
    if (endWidg)
      stateLayout->addRow(tr("End   "), endWidg);
  }

  newProc->addContent(stateWidget);

  // Global setup
  newProc->expand(expanded);
  m_processesSectionWidgets.push_back(newProc); // add in list
  this->layout()->addWidget(newProc);           // add in view
}

void ProcessTabWidget::updateDisplayedValues()
{
  for (auto process : m_processesSectionWidgets)
  {
    delete process;
  }
  m_processesSectionWidgets.clear();

  const ConstraintModel& cst = m_constraintWidget.model();
  const auto fv = isInFullView(cst);
  auto sv_rack = !fv ? &cst.smallView() : nullptr;

  if(fv)
  {
    // If the selected constraint is the full view one :
    // all widgets are expanded.
    for(const auto& p : cst.processes)
    {
      displayProcess(p, true);
    }
  }
  else
  {
    // Else, if the selected constraint is just part of a scenario :
    // The widget is expanded if the process is the edited one in
    // one of the slots.
    if(!sv_rack)
    {
      for(const auto& p : cst.processes)
      {
        displayProcess(p, false);
      }
    }
    else
    {
      // List all the processes with their state.
      boost::container::flat_map<Id<Process::ProcessModel>, bool> expanded;
      expanded.reserve(cst.processes.size());
      for(const auto& process : cst.processes)
      {
        expanded.insert(std::make_pair(process.id(), false));
      }

      for(const auto& slot : cst.smallView())
      {
        if(auto lay = slot.frontProcess)
        {
          expanded[*lay] = true;
        }
      }

      for(const auto& p : cst.processes)
      {
        displayProcess(p, expanded[p.id()]);
      }
    }
  }
}

void ProcessTabWidget::ask_processNameChanged(
    const Process::ProcessModel& p, QString s)
{
  if (s != p.metadata().getName())
  {
    auto cmd = new Command::ChangeElementName<Process::ProcessModel>{p, s};
    emit m_constraintWidget.commandDispatcher()->submitCommand(cmd);
  }
}

void ProcessTabWidget::createLayerInNewSlot(
    const Id<Process::ProcessModel>& processId)
{
  auto cmd
      = new Command::AddLayerInNewSlot{m_constraintWidget.model(), processId};

  m_constraintWidget.commandDispatcher()->submitCommand(cmd);
}
}
