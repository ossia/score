#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/DialogWidget/MessageTreeView.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/State/AddStateProcess.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Process/ProcessList.hpp>
#include <Process/StateProcess.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QtAlgorithms>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QLabel>
#include <QVector>
#include <QWidget>
#include <QLineEdit>

#include <QMenu>
#include <algorithm>

#include <Inspector/InspectorWidgetBase.hpp>
#include "StateInspectorWidget.hpp"
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <QSizePolicy>
#include <iscore/selection/SelectionDispatcher.hpp>
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
StateInspectorWidget::StateInspectorWidget(
        const StateModel& object,
        const iscore::DocumentContext& doc,
        QWidget *parent):
    QWidget {parent},
    m_model {object},
    m_context{doc},
    m_commandDispatcher(new CommandDispatcher<>{m_context.commandStack}),
    m_selectionDispatcher(new iscore::SelectionDispatcher{m_context.selectionStack})
{
    setObjectName("StateInspectorWidget");
    setParent(parent);

    auto lay = new iscore::MarginLess<QVBoxLayout>{this};
    this->setLayout(lay);

    updateDisplayedValues();
    m_model.stateProcesses.added.connect<StateInspectorWidget, &StateInspectorWidget::on_stateProcessCreated>(this);
    m_model.stateProcesses.removed.connect<StateInspectorWidget, &StateInspectorWidget::on_stateProcessRemoved>(this);
}

void StateInspectorWidget::updateDisplayedValues()
{
    // Cleanup
    // OPTIMIZEME
    qDeleteAll(m_properties);
    m_properties.clear();

    auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
    ISCORE_ASSERT(scenar);

    auto event = m_model.eventId();

    // State setup
    m_stateSection = new Inspector::InspectorSectionWidget{"State", false, this};
    auto tv = new MessageTreeView{m_model,
                                m_stateSection};

    m_stateSection->addContent(tv);

    auto linkWidget = new QWidget;
    auto linkLay = new iscore::MarginLess<QHBoxLayout>{linkWidget};

    // Constraints setup
    if(m_model.previousConstraint())
    {
        auto btn = SelectionButton::make(
                    tr("Prev. Constraint"),
                &scenar->constraint(m_model.previousConstraint()),
                selectionDispatcher(),
                this);

        linkLay->addWidget(btn);
    }
    if(m_model.nextConstraint())
    {
        auto btn = SelectionButton::make(
                    tr("Next Constraint"),
                &scenar->constraint(m_model.nextConstraint()),
                selectionDispatcher(),
                this);

        linkLay->addWidget(btn);
    }
    linkLay->addStretch(1);

    m_stateSection->addContent(linkWidget);

    // State processes
    auto procWidg = new QWidget;
    auto procLay = new QVBoxLayout;
    {
        auto addProcButton = new QPushButton;
        addProcButton->setText("+");
        addProcButton->setObjectName("addAProcess");
        procLay->addWidget(addProcButton);

        auto addProcText = new QLabel(tr("Add Process"));
        addProcText->setStyleSheet(QString("text-align : left;"));
        procLay->addWidget(addProcText);

        // add new process dialog
        auto addProcess = new AddStateProcessDialog {
                m_context.app.components.factory<Process::StateProcessList>(),
                this};

        // CONNECTIONS
        connect(addProcButton,  &QPushButton::pressed,
            addProcess, &AddStateProcessDialog::launchWindow);

        connect(addProcess, &AddStateProcessDialog::okPressed,
            this, &StateInspectorWidget::createStateProcess);

        for(auto& proc : m_model.stateProcesses)
        {
            procLay->addWidget(displayStateProcess(proc));
        }

        procWidg->setLayout(procLay);
    }

    m_stateSection->addContent(procWidg);
    m_properties.push_back(m_stateSection);

    for(auto w : m_properties)
        this->layout()->addWidget(w);

}

void StateInspectorWidget::splitEvent()
{
    auto scenar = dynamic_cast<const Scenario::ScenarioModel*>(m_model.parent());
    if (scenar)
    {
        auto& parentEvent = scenar->events.at(m_model.eventId());
        if(parentEvent.states().size() > 1)
        {
            auto cmd = new Scenario::Command::SplitEvent{
                        *scenar,
                        m_model.eventId(),
                        {m_model.id()}};

            m_commandDispatcher->submitCommand(cmd);
        }
    }
}

void StateInspectorWidget::on_stateProcessCreated(const Process::StateProcess &)
{
    updateDisplayedValues();
}

void StateInspectorWidget::on_stateProcessRemoved(const Process::StateProcess &)
{
    updateDisplayedValues();
}


void StateInspectorWidget::createStateProcess(
        const UuidKey<Process::StateProcessFactory> & key)
{
    auto cmd = new Command::AddStateProcessToState(m_model, key);
    m_commandDispatcher->submitCommand(cmd);
}

Inspector::InspectorSectionWidget*
   StateInspectorWidget::displayStateProcess(
        const Process::StateProcess& process)
{
    using namespace iscore;

    // New Section
    auto sectionWidg = new Inspector::InspectorSectionWidget(process.prettyName(), true);
    sectionWidg->showMenu(true);

    const auto& fact = m_context.app.components.factory<Process::StateProcessInspectorWidgetDelegateFactoryList>();
    if(auto widg = fact.make(&Process::StateProcessInspectorWidgetDelegateFactory::make,
                             process, m_context, sectionWidg))
    {
        sectionWidg->addContent(widg);
    }

    // delete process
    ISCORE_TODO_("Delete state process");


    auto delAct = sectionWidg->menu()->addAction(tr("Remove State Process"));
    connect(delAct, &QAction::triggered,
            this, [=,id=process.id()] ()
        {
            auto cmd = new Command::RemoveStateProcess{iscore::IDocument::path(m_model), id};
            emit m_commandDispatcher->submitCommand(cmd);
        }, Qt::QueuedConnection);

    return sectionWidg;

}
}
