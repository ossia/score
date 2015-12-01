#include <Inspector/InspectorWidgetList.hpp>
#include <Inspector/Separator.hpp>
#include <Process/Process.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/RemoveProcessFromConstraint.hpp>
#include <Scenario/Commands/Constraint/SetLooping.hpp>
#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/SetProcessDuration.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/optional/optional.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QFormLayout>
#include <QtGlobal>
#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>
#include <utility>

#include "ConstraintInspectorWidget.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include "Widgets/DurationSectionWidget.hpp"
#include "Widgets/Rack/RackInspectorSection.hpp"
#include "Widgets/RackWidget.hpp"
#include <core/application/ApplicationComponents.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/document/DocumentContext.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

using namespace Scenario::Command;
using namespace iscore;
using namespace iscore::IDocument;


ConstraintInspectorWidget::ConstraintInspectorWidget(
        const InspectorWidgetList& widg,
        const ProcessList& pl,
        const ConstraintModel& object,
        std::unique_ptr<ConstraintInspectorDelegate> del,
        const iscore::DocumentContext& ctx,
        QWidget* parent) :
    InspectorWidgetBase{object, ctx, parent},
    m_widgetList{widg},
    m_processList{pl},
    m_model{object},
    m_delegate{std::move(del)}
{
    setObjectName("Constraint");

    ////// HEADER
    // metadata
    m_metadata = new MetadataWidget{
                 &m_model.metadata,
                 commandDispatcher(),
                 &m_model,
                 this};
    m_metadata->setType(ConstraintModel::description());

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    m_delegate->addWidgets_pre(m_properties, this);

    ////// BODY
    QPushButton* setAsDisplayedConstraint = new QPushButton {tr("Full view"), this};
    connect(setAsDisplayedConstraint, &QPushButton::clicked,
            this, [this] {
        auto& base = get<ScenarioDocumentModel> (*documentFromObject(m_model));

        base.setDisplayedConstraint(model());
    });

    m_properties.push_back(setAsDisplayedConstraint);

    // Events
    if(auto scenario = qobject_cast<Scenario::ScenarioModel*>(m_model.parent()))
    {
        m_properties.push_back(makeStatesWidget(scenario));
    }

    // Separator
    m_properties.push_back(new Separator {this});

    // Durations
    auto& ctrl = ctx.app.components.applicationPlugin<ScenarioApplicationPlugin>();
    m_durationSection = new DurationSectionWidget {ctrl.editionSettings(), *m_delegate, this};
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
    m_processSection = new InspectorSectionWidget("Processes", false, this);
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
    auto addProcess = new AddProcessDialog {m_processList, this};

    connect(addProcButton,  &QToolButton::pressed,
            addProcess, &AddProcessDialog::launchWindow);

    m_properties.push_back(addProc);

    connect(addProcess, &AddProcessDialog::okPressed,
            this, &ConstraintInspectorWidget::createProcess);

    // Separator
    m_properties.push_back(new Separator {this});

    // Rackes
    m_rackSection = new InspectorSectionWidget {"Rackes", false, this};
    m_rackSection->setObjectName("Rackes");
    m_rackSection->expand();

    m_rackWidget = new RackWidget {this};

    m_properties.push_back(m_rackSection);
    m_properties.push_back(m_rackWidget);

    // Plugins
    for(auto& plugdata : m_model.pluginModelList.list())
    {
        for(auto plugin : ctx.pluginModels())
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

    m_delegate->addWidgets_post(m_properties, this);

    // Display data
    updateAreaLayout(m_properties);
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

    m_delegate->updateElements();
}

void ConstraintInspectorWidget::createProcess(const ProcessFactoryKey& processName)
{
    auto cmd = make_AddProcessToConstraint(model(), processName);
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
        for (auto& r : m_model.racks)
        {
            if(r.metadata.name() == rack)
            {
                auto id = r.id();
                auto cmd = new ShowRackInViewModel(*vm, id);
                emit commandDispatcher()->submitCommand(cmd);
            }
        }
    }
}

void ConstraintInspectorWidget::displaySharedProcess(const Process& process)
{
    auto newProc = new InspectorSectionWidget(process.metadata.name(), true);
    connect(newProc, &InspectorSectionWidget::nameChanged,
            this, [&] (QString s)
    {
        ask_processNameChanged(process, s);
    });
    con(process.metadata, &ModelMetadata::nameChanged,
            newProc, &InspectorSectionWidget::renameSection);

    // Process
    auto processWidget = m_widgetList.makeInspectorWidget(
                             process.objectName(), process, newProc); // FIXME objectName BERK
    newProc->addContent(processWidget);

    // Start & end state
    QWidget* stateWidget = new QWidget;
    QFormLayout* stateLayout = new QFormLayout;
    stateLayout->setSpacing(0);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    stateWidget->setLayout(stateLayout);

    if(auto start = process.startStateData())
    {
        auto startWidg = m_widgetList.makeInspectorWidget(
                             start->stateName(), *start, newProc);
        stateLayout->addRow(tr("Start state"), startWidg);
    }

    if(auto end = process.endStateData())
    {
        auto endWidg = m_widgetList.makeInspectorWidget(
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
                                    rack.metadata.name(),
                                    rack,
                                    this};

    m_rackesSectionWidgets[rack.id()] = newRack;
    m_rackSection->addContent(newRack);
}

void ConstraintInspectorWidget::ask_processNameChanged(const Process& p, QString s)
{
    if(s != p.metadata.name())
    {
        auto cmd = new ChangeElementName<Process>{path(p), s};
        emit commandDispatcher()->submitCommand(cmd);
    }
}

QWidget* ConstraintInspectorWidget::makeStatesWidget(Scenario::ScenarioModel* scenar)
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
