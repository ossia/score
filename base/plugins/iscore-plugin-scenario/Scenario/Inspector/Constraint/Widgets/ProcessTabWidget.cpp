#include "ProcessTabWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetList.hpp>

#include <iscore/widgets/SpinBoxes.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/document/DocumentContext.hpp>

#include <QAction>
#include <QBoxLayout>
#include <QFormLayout>
#include <QToolButton>

#include <Scenario/DialogWidget/AddProcessDialog.hpp>

#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Constraint/RemoveProcessFromConstraint.hpp>
#include <Scenario/Commands/SetProcessDuration.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>

#include <Process/Process.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <Process/Inspector/ProcessInspectorWidget.hpp>

namespace Scenario {

ProcessTabWidget::ProcessTabWidget(const ConstraintInspectorWidget& parentCstr, QWidget *parent) :
    QWidget(parent),
    m_constraintWidget{parentCstr}
{
    // CREATION

    // main layout
    auto processesLay = new iscore::MarginLess<QVBoxLayout>{this};

    // usefull ?
    m_processSection = new Inspector::InspectorSectionWidget("Processes", false, this);
    m_processSection->setObjectName("Processes");

    // add new process widget
    QWidget* addProc = new QWidget(this);
    QHBoxLayout* addProcLayout = new iscore::MarginLess<QHBoxLayout>{addProc};

    QToolButton* addProcButton = new QToolButton;
    addProcButton->setText("+");
    addProcButton->setObjectName("addAProcess");
    addProcButton->setAutoRaise(true);

    auto addProcText = new QLabel("Add Process");
    addProcText->setStyleSheet(QString("text-align : left;"));

    // add new process dialog
    auto addProcess = new AddProcessDialog {m_constraintWidget.processList(), this};

    // CONNECTIONS
    connect(addProcButton,  &QToolButton::pressed,
        addProcess, &AddProcessDialog::launchWindow);

    connect(addProcess, &AddProcessDialog::okPressed,
        this, &ProcessTabWidget::createProcess);

    // LAYOUTS
    addProcLayout->addWidget(addProcButton);
    addProcLayout->addWidget(addProcText);

    processesLay->addWidget(addProc);
    processesLay->addWidget(m_processSection);
    processesLay->addStretch(1);
}

void ProcessTabWidget::createProcess(const ProcessFactoryKey& processName)
{
    auto cmd = Command::make_AddProcessToConstraint(m_constraintWidget.model(), processName);
    m_constraintWidget.commandDispatcher()->submitCommand(cmd);
}
void ProcessTabWidget::displaySharedProcess(const Process::ProcessModel& process)
{
    using namespace iscore;

    // New Section
    auto newProc = new Inspector::InspectorSectionWidget(process.metadata.name(), true);

    // name changing connections
    connect(newProc, &Inspector::InspectorSectionWidget::nameChanged,
            this, [&] (QString s)
        {
            ask_processNameChanged(process, s);
        });
    con(process.metadata, &ModelMetadata::nameChanged,
        newProc, &Inspector::InspectorSectionWidget::renameSection);

    // ***********************
    // PROCESS

    // add view in new slot
    newProc->showMenu(true);

    const auto& fact = m_constraintWidget.context().app.components.factory<ProcessInspectorWidgetDelegateFactoryList>();
    if(auto widg = fact.make(process, m_constraintWidget.context(), newProc))
    {
        newProc->addContent(widg);

        auto act = newProc->menu()->addAction(tr("Display in new slot"));

        connect(act, &QAction::triggered,
                this, [&] () { createLayerInNewSlot(process.id()); });
    }

    // delete process
    newProc->enableDelete();
    connect(newProc, &Inspector::InspectorSectionWidget::deletePressed,
            this, [=,id=process.id()] ()
        {
            auto cmd = new Command::RemoveProcessFromConstraint{iscore::IDocument::path(m_constraintWidget.model()), id};
            emit m_constraintWidget.commandDispatcher()->submitCommand(cmd);
        });


    // Start & end state
    QWidget* stateWidget = new QWidget;
    QFormLayout* stateLayout = new iscore::MarginLess<QFormLayout>{stateWidget};

    if(auto start = process.startStateData())
    {
        auto startWidg = m_constraintWidget.widgetList().makeInspectorWidget(*start, newProc);
        stateLayout->addRow(tr("Start "), startWidg);
    }

    if(auto end = process.endStateData())
    {
        auto endWidg = m_constraintWidget.widgetList().makeInspectorWidget(*end, newProc);
        stateLayout->addRow(tr("End   "), endWidg);
    }

    newProc->addContent(stateWidget);

    // Durations
    QPointer<TimeSpinBox> durWidg = new TimeSpinBox;
    durWidg->setTime(process.duration().toQTime());
    con(process, &Process::ProcessModel::durationChanged,
        this, [=] (const TimeValue& tv)
    {
        if(durWidg)
            durWidg->setTime(tv.toQTime());
    });
    connect(durWidg, &TimeSpinBox::editingFinished,
            this, [&,durWidg]
    {
        auto cmd = new Command::SetProcessDuration{process, TimeValue(durWidg->time())};
        m_constraintWidget.commandDispatcher()->submitCommand(cmd);
    });
    stateLayout->addRow(tr("Duration"), durWidg);


    // Global setup
    m_processesSectionWidgets.push_back(newProc);   // add in list
    m_processSection->addContent(newProc);  // add in view
}

void ProcessTabWidget::updateDisplayedValues()
{
    for(auto& process : m_processesSectionWidgets)
    {
        m_processSection->removeContent(process);
    }
    m_processesSectionWidgets.clear();

    for(const auto& process : m_constraintWidget.model().processes)
    {
        displaySharedProcess(process);
    }

}

void ProcessTabWidget::ask_processNameChanged(const Process::ProcessModel& p, QString s)
{
    if(s != p.metadata.name())
    {
        auto cmd = new Command::ChangeElementName<Process::ProcessModel>{iscore::IDocument::path(p), s};
        emit m_constraintWidget.commandDispatcher()->submitCommand(cmd);
    }
}

void ProcessTabWidget::createLayerInNewSlot(const Id<Process::ProcessModel>& processId)
{
    auto cmd = new Command::AddLayerInNewSlot{m_constraintWidget.model(), processId};

    m_constraintWidget.commandDispatcher()->submitCommand(cmd);
}

}
