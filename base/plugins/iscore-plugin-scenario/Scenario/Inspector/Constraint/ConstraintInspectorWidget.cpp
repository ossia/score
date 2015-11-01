#include "ConstraintInspectorWidget.hpp"
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include "Widgets/RackWidget.hpp"
#include "Widgets/DurationSectionWidget.hpp"
#include "Widgets/Rack/RackInspectorSection.hpp"

#include <Scenario/Commands/SetProcessDuration.hpp>
#include <Scenario/Commands/Constraint/SetLooping.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/RemoveProcessFromConstraint.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Process/Process.hpp>

#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>

#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <Inspector/Separator.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <QFrame>
#include <QLineEdit>
#include <QLayout>
#include <QLabel>
#include <QFormLayout>
#include <QToolButton>
#include <QCheckBox>
#include <QPushButton>
using namespace Scenario::Command;
using namespace iscore;
using namespace iscore::IDocument;

ConstraintInspectorWidget::ConstraintInspectorWidget(
        const ConstraintModel& object,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase{object, doc, parent},
    m_model{object}
{
    setObjectName("Constraint");

    ////// HEADER
    // metadata
    m_metadata = new MetadataWidget{
                 &m_model.metadata,
                 commandDispatcher(),
                 &m_model,
                 this};
    m_metadata->setType(ConstraintModel::prettyName());

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    if(m_model.objectName() == "BaseConstraintModel")
    {
        auto scenario = m_model.parentScenario();
        auto& tn = scenario->timeNode(m_model.endTimeNode());
        m_triggerLine = new TriggerInspectorWidget{tn, this};
        m_triggerLine->HideRmButton();
        m_properties.push_back(new QLabel(tr("Trigger")));
        m_properties.push_back(m_triggerLine);
    }

    ////// BODY
    QPushButton* setAsDisplayedConstraint = new QPushButton {tr("Full view"), this};
    connect(setAsDisplayedConstraint, &QPushButton::clicked,
            this, [this] {
        auto& base = get<BaseElementModel> (*documentFromObject(m_model));

        base.setDisplayedConstraint(model());
    });

    m_properties.push_back(setAsDisplayedConstraint);

    // Events
    if(auto scenario = qobject_cast<ScenarioModel*>(m_model.parent()))
    {
        m_properties.push_back(makeStatesWidget(scenario));
    }

    // Separator
    m_properties.push_back(new Separator {this});

    // Durations
    m_durationSection = new DurationSectionWidget {this};
    m_properties.push_back(m_durationSection);
    auto loop = new QCheckBox{tr("Loop content"), this};
    loop->setChecked(m_model.looping());
    connect(loop, &QCheckBox::clicked,
            this, [this] (bool checked){
        auto cmd = new SetLooping{m_model, checked};
        commandDispatcher()->submitCommand(cmd);
    });
    m_properties.push_back(loop);

    // Separator
    m_properties.push_back(new Separator {this});

    // Processes
    m_processSection = new InspectorSectionWidget("Processes", this);
    m_processSection->setObjectName("Processes");

    m_properties.push_back(m_processSection);

    QWidget* addProc = new QWidget(this);
    QHBoxLayout* addProcLayout = new QHBoxLayout;
    addProcLayout->setContentsMargins(0, 0, 0 , 0);
    addProc->setLayout(addProcLayout);
    // Button
    QToolButton* addProcButton = new QToolButton;
    addProcButton->setText("+");
    addProcButton->setObjectName("addAProcess");

    // Text
    auto addProcText = new QLabel("Add Process");
    addProcText->setStyleSheet(QString("text-align : left;"));

    addProcLayout->addWidget(addProcButton);
    addProcLayout->addWidget(addProcText);
    auto addProcess = new AddProcessDialog {this};

    connect(addProcButton,  &QToolButton::pressed,
            addProcess, &AddProcessDialog::launchWindow);

    m_properties.push_back(addProc);

    connect(addProcess, &AddProcessDialog::okPressed,
            this, &ConstraintInspectorWidget::createProcess);

    // Separator
    m_properties.push_back(new Separator {this});

    // Rackes
    m_rackSection = new InspectorSectionWidget {"Rackes", this};
    m_rackSection->setObjectName("Rackes");
    m_rackSection->expand();

    m_rackWidget = new RackWidget {this};

    m_properties.push_back(m_rackSection);
    m_properties.push_back(m_rackWidget);

    // Plugins
    for(auto& plugdata : m_model.pluginModelList.list())
    {
        for(iscore::DocumentDelegatePluginModel* plugin : doc.model().pluginModels())
        {
            auto md = plugin->makeElementPluginWidget(plugdata, this);
            if(md)
            {
                m_properties.push_back(md);
                break;
            }
        }
    }

    updateDisplayedValues();

    // Display data
    updateAreaLayout(m_properties);

    if(m_model.objectName() == "BaseConstraintModel")
    {
        auto scenario = m_model.parentScenario();
        auto& tn = scenario->timeNode(m_model.endTimeNode());
        auto trWidg = new TriggerInspectorWidget{tn, this};
        trWidg->HideRmButton();
        m_properties.push_back(trWidg);
    }

}

const ConstraintModel& ConstraintInspectorWidget::model() const
{
    return m_model;
}

void ConstraintInspectorWidget::updateDisplayedValues()
{
    // Cleanup the widgets
    for(auto& process : m_processesSectionWidgets)
    {
        m_processSection->removeContent(process);
    }

    m_processesSectionWidgets.clear();

    for(auto& rack_pair : m_rackesSectionWidgets)
    {
        m_rackSection->removeContent(rack_pair.second);
    }

    m_rackesSectionWidgets.clear();

    // Cleanup the connections
    for(auto& connection : m_connections)
    {
        QObject::disconnect(connection);
    }

    m_connections.clear();

    // Constraint interface
    m_connections.push_back(
                con(model().processes, &NotifyingMap<Process>::added,
                    this, &ConstraintInspectorWidget::on_processCreated));
    m_connections.push_back(
                con(model().processes, &NotifyingMap<Process>::removed,
                    this, &ConstraintInspectorWidget::on_processRemoved));
    m_connections.push_back(
                con(model().racks, &NotifyingMap<RackModel>::added,
                    this, &ConstraintInspectorWidget::on_rackCreated));
    m_connections.push_back(
                con(model().racks, &NotifyingMap<RackModel>::removed,
                    this, &ConstraintInspectorWidget::on_rackRemoved));

    m_connections.push_back(
                con(model(), &ConstraintModel::viewModelCreated,
                    this, &ConstraintInspectorWidget::on_constraintViewModelCreated));
    m_connections.push_back(
                con(model(), &ConstraintModel::viewModelRemoved,
                    this, &ConstraintInspectorWidget::on_constraintViewModelRemoved));

    // Processes
    for(const auto& process : model().processes)
    {
        displaySharedProcess(process);
    }

    // Rack
    for(const auto& rack : model().racks)
    {
        setupRack(rack);
    }

    if(m_model.objectName() == "BaseConstraintModel")
    {
        auto scenario = m_model.parentScenario();
        auto& tn = scenario->timeNode(m_model.endTimeNode());
        m_triggerLine->updateExpression(tn.trigger()->expression().toString() );
    }

}

void ConstraintInspectorWidget::createProcess(QString processName)
{
    auto cmd = new AddProcessToConstraint{model(), processName};
    commandDispatcher()->submitCommand(cmd);
}

void ConstraintInspectorWidget::createRack()
{
    auto cmd = new AddRackToConstraint{model()};
    commandDispatcher()->submitCommand(cmd);
}

void ConstraintInspectorWidget::createLayerInNewSlot(QString processName)
{
    // TODO this will bite us when the name does not contain the id anymore.
    // We will have to stock the id's somewhere.
    auto cmd = new AddLayerInNewSlot{model(), Id<Process>(processName.toInt())};

    commandDispatcher()->submitCommand(cmd);
}

void ConstraintInspectorWidget::activeRackChanged(QString rack, ConstraintViewModel* vm)
{
    // TODO mettre à jour l'inspecteur si la rack affichée change (i.e. via une commande réseau).
    if (m_rackWidget == nullptr)
        return;

    if(rack == m_rackWidget->hiddenText)
    {
        if(vm->isRackShown())
        {
            auto cmd = new HideRackInViewModel(*vm);
            emit commandDispatcher()->submitCommand(cmd);
        }
    }
    else
    {
        bool ok {};
        auto id = Id<RackModel> (rack.toInt(&ok));

        if(ok)
        {
            auto cmd = new ShowRackInViewModel(*vm, id);
            emit commandDispatcher()->submitCommand(cmd);
        }
    }
}

void ConstraintInspectorWidget::displaySharedProcess(const Process& process)
{
    InspectorSectionWidget* newProc = new InspectorSectionWidget(process.metadata.name());

    // Process
    auto processWidget = InspectorWidgetList::makeInspectorWidget(
                             process.processName(), process, newProc);
    newProc->addContent(processWidget);

    // Start & end state
    QWidget* stateWidget = new QWidget;
    QFormLayout* stateLayout = new QFormLayout;
    stateLayout->setSpacing(0);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    stateWidget->setLayout(stateLayout);

    if(auto start = process.startState())
    {
        auto startWidg = InspectorWidgetList::makeInspectorWidget(
                             start->stateName(), *start, newProc);
        stateLayout->addRow(tr("Start state"), startWidg);
    }

    if(auto end = process.endState())
    {
        auto endWidg = InspectorWidgetList::makeInspectorWidget(
                           end->stateName(), *end, newProc);
        stateLayout->addRow(tr("End state"), endWidg);
    }
    newProc->addContent(stateWidget);

    QPointer<TimeSpinBox> durWidg = new TimeSpinBox;
    durWidg->setTime(process.duration().toQTime());
    con(process, &Process::durationChanged,
        this, [=] (const TimeValue& tv) {
        if(durWidg)
            durWidg->setTime(tv.toQTime());
    });
    connect(durWidg, &TimeSpinBox::editingFinished,
        this, [&,durWidg] {
        auto cmd = new SetProcessDuration{process, TimeValue(durWidg->time())};
        commandDispatcher()->submitCommand(cmd);
    });
    stateLayout->addRow(tr("Duration"), durWidg);

    // Delete button
    auto deleteButton = new QPushButton{tr("Delete")};
    connect(deleteButton, &QPushButton::pressed, this, [=,id=process.id()] ()
    {
        auto cmd = new RemoveProcessFromConstraint{iscore::IDocument::path(model()), id};
        emit commandDispatcher()->submitCommand(cmd);
    });
    newProc->addContent(deleteButton);

    // Global setup
    m_processesSectionWidgets.push_back(newProc);
    m_processSection->addContent(newProc);

    connect(processWidget,   SIGNAL(createViewInNewSlot(QString)),
            this,   SLOT(createLayerInNewSlot(QString)));
}

void ConstraintInspectorWidget::setupRack(const RackModel& rack)
{
    // Display the widget
    RackInspectorSection* newRack = new RackInspectorSection {
                                    QString{"Rack.%1"} .arg(*rack.id().val()),
                                    rack,
                                    this};

    m_rackesSectionWidgets[rack.id()] = newRack;
    m_rackSection->addContent(newRack);
}

QWidget* ConstraintInspectorWidget::makeStatesWidget(ScenarioModel* scenar)
{
    QWidget* eventWid = new QWidget{this};
    QFormLayout* eventLay = new QFormLayout {eventWid};
    eventLay->setVerticalSpacing(0);

    if(auto sst = m_model.startState())
    {
        auto btn = SelectionButton::make(
                       tr("Start State"),
                       &scenar->state(sst),
                       selectionDispatcher(),
                       this);
        eventLay->addWidget(btn);
    }

    if(auto est = m_model.endState())
    {
        auto btn = SelectionButton::make(
                    tr("End State"),
                       &scenar->state(est),
                       selectionDispatcher(),
                       this);
        eventLay->addWidget(btn);
    }

    return eventWid;
}

void ConstraintInspectorWidget::on_processCreated(
        const Process&)
{
    // OPTIMIZEME
    updateDisplayedValues();
}

void ConstraintInspectorWidget::on_processRemoved(
        const Process&)
{
    // OPTIMIZEME
    updateDisplayedValues();
}


void ConstraintInspectorWidget::on_rackCreated(
        const RackModel& rack)
{
    setupRack(rack);
    m_rackWidget->viewModelsChanged();
}

void ConstraintInspectorWidget::on_rackRemoved(const RackModel& rack)
{
    auto ptr = m_rackesSectionWidgets[rack.id()];
    m_rackesSectionWidgets.erase(rack.id());

    if(ptr)
    {
        ptr->deleteLater();
    }

    m_rackWidget->viewModelsChanged();
}

void ConstraintInspectorWidget::on_constraintViewModelCreated(const ConstraintViewModel&)
{
    // OPTIMIZEME
    m_rackWidget->viewModelsChanged();
}

void ConstraintInspectorWidget::on_constraintViewModelRemoved(const QObject*)
{
    // OPTIMIZEME
    m_rackWidget->viewModelsChanged();
}
