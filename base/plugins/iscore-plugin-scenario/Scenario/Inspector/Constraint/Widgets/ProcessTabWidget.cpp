#include "ProcessTabWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetList.hpp>

#include <iscore/document/DocumentContext.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include <Scenario/DialogWidget/AddProcessDialog.hpp>

#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/RemoveProcessFromConstraint.hpp>
#include <Scenario/Commands/Constraint/SetProcessUseParentDuration.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/SetProcessDuration.hpp>

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

#include <iscore/widgets/SetIcons.hpp>
#include <iscore/widgets/TextLabel.hpp>

namespace Scenario
{

ProcessTabWidget::ProcessTabWidget(
    const ConstraintInspectorWidget& parentCstr, QWidget* parent)
    : QWidget(parent), m_constraintWidget{parentCstr}
{
  // CREATION

  // main layout
  auto processesLay = new iscore::MarginLess<QVBoxLayout>{this};

  // usefull ?
  m_processSection
      = new Inspector::InspectorSectionWidget("Processes", false, this);
  m_processSection->setObjectName("Processes");

  // add new process widget
  auto addProc = new QWidget(this);
  auto addProcLayout = new iscore::MarginLess<QHBoxLayout>{addProc};

  auto addProcButton = new QToolButton;
  addProcButton->setText("+");
  QIcon addIcon = makeIcons(
      ":/icons/condition_add_on.png", ":/icons/condition_add_off.png");
  addProcButton->setIcon(addIcon);
  addProcButton->setObjectName("addAProcess");
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
  processesLay->addWidget(m_processSection);
  processesLay->addStretch(1);
}

void ProcessTabWidget::createProcess(
    const UuidKey<Process::ProcessModelFactory>& processName)
{
  auto cmd = Command::make_AddProcessToConstraint(
      m_constraintWidget.model(), processName);
  m_constraintWidget.commandDispatcher()->submitCommand(cmd);
}

void ProcessTabWidget::displaySharedProcess(
    const Process::ProcessModel& process)
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
            .app.components
            .interfaces<Process::InspectorWidgetDelegateFactoryList>();
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
      QString(":/icons/delete_on.png"), QString(":/icons/delete_off.png")));
  connect(
      delAct, &QAction::triggered, this, [ =, id = process.id() ]() {
        auto cmd = new Command::RemoveProcessFromConstraint{
            m_constraintWidget.model(), id};
        emit m_constraintWidget.commandDispatcher()->submitCommand(cmd);
      },
      Qt::QueuedConnection);

  auto setParentDurAct = newProc->menu()->addAction(tr("Use parent duration"));
  setParentDurAct->setCheckable(true);
  setParentDurAct->setChecked(process.useParentDuration());
  con(process, &Process::ProcessModel::useParentDurationChanged, this,
      [=](bool b) {
        if (setParentDurAct->isChecked() != b)
        {
          setParentDurAct->setChecked(b);
        }
      });

  connect(setParentDurAct, &QAction::toggled, this, [&](bool b) {
    if (b != process.useParentDuration())
    {
      auto cmd = new Command::SetProcessUseParentDuration{process, b};
      m_constraintWidget.commandDispatcher()->submitCommand(cmd);
    }
  });

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
  m_processesSectionWidgets.push_back(newProc); // add in list
  m_processSection->addContent(newProc);        // add in view
}

void ProcessTabWidget::updateDisplayedValues()
{
  for (auto& process : m_processesSectionWidgets)
  {
    m_processSection->removeContent(process);
  }
  m_processesSectionWidgets.clear();

  for (const auto& process : m_constraintWidget.model().processes)
  {
    displaySharedProcess(process);
  }
}
template <typename T>
std::vector<std::string> brethrenNames(const T& vec)
{
  std::vector<std::string> names;
  for (auto& elt : vec)
  {
    names.push_back(elt.metadata.getName().toStdString());
  }
  return names;
}

void ProcessTabWidget::ask_processNameChanged(
    const Process::ProcessModel& p, QString s)
{
  if (s != p.metadata().getName())
  {
    auto cmd = new Command::ChangeElementName<Process::ProcessModel>{
        iscore::IDocument::path(p), s};
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
