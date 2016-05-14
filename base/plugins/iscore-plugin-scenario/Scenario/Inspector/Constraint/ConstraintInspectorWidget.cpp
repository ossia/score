#include <Inspector/InspectorWidgetList.hpp>
#include <Inspector/Separator.hpp>
#include <Process/Process.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Constraint/SetLooping.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QFormLayout>
#include <QtGlobal>
#include <QSlider>
#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>
#include <QTabWidget>
#include <utility>

#include "ConstraintInspectorWidget.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include "Widgets/DurationSectionWidget.hpp"
#include "Widgets/Rack/RackInspectorSection.hpp"
#include "Widgets/RackWidget.hpp"
#include <Scenario/Inspector/Constraint/Widgets/ProcessTabWidget.hpp>
#include <Scenario/Inspector/Constraint/Widgets/ProcessViewTabWidget.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
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
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

namespace Scenario
{
ConstraintInspectorWidget::ConstraintInspectorWidget(
        const Inspector::InspectorWidgetList& widg,
        const Process::ProcessList& pl,
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
    using namespace iscore;
    using namespace iscore::IDocument;
    setObjectName("Constraint");

    ////// HEADER
    // metadata
    m_metadata = new MetadataWidget{
                 &m_model.metadata,
                 commandDispatcher(),
                 &m_model,
                 this};

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    auto speedWidg = new QWidget{this};
    auto speedLay = new iscore::MarginLess<QVBoxLayout>{speedWidg};

    QSlider* speedSlider = new QSlider{Qt::Horizontal};
    speedSlider->setTickInterval(100);
    speedSlider->setMinimum(-100);
    speedSlider->setMaximum(500);
    speedSlider->setValue(m_model.duration.executionSpeed() * 100);
    auto speedLab = new QLabel{"Speed x" + QString::number(double(speedSlider->value())/100.0)};

    speedLay->addWidget(speedLab);
    speedLay->addWidget(speedSlider);

    connect(speedSlider, &QSlider::valueChanged,
            this, [=] (int val) {
        // TODO command
        ((ConstraintModel&)(m_model)).duration.setExecutionSpeed(double(val) / 100.0);
        speedLab->setText("Speed x" + QString::number(double(val)/100.0));
    });

    m_properties.push_back(speedWidg);

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
    m_properties.push_back(new Inspector::HSeparator {this});

    // Durations
    auto& ctrl = ctx.app.components.applicationPlugin<ScenarioApplicationPlugin>();
    m_durationSection = new DurationSectionWidget {ctrl.editionSettings(), *m_delegate, this};
    m_properties.push_back(m_durationSection);
    auto loop = new QCheckBox{tr("Loop content"), this};
    loop->setChecked(m_model.looping());
    connect(loop, &QCheckBox::clicked,
            this, [this] (bool checked){
        auto cmd = new Command::SetLooping{m_model, checked};
        commandDispatcher()->submitCommand(cmd);
    });
    m_properties.push_back(loop);

    // Separator
    m_properties.push_back(new Inspector::HSeparator {this});

    // tab Processes/View
    auto tabWidget = new QTabWidget{this};
    m_processesTabPage = new ProcessTabWidget{ *this, this};
    m_viewTabPage = new ProcessViewTabWidget{*this, this};

    tabWidget->addTab(m_processesTabPage, tr("Processes"));
    tabWidget->addTab(m_viewTabPage, tr("View"));

    m_properties.push_back(tabWidget);

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

    // Constraint interface
    model().processes.added.connect<ConstraintInspectorWidget, &ConstraintInspectorWidget::on_processCreated>(this);
    model().processes.removed.connect<ConstraintInspectorWidget, &ConstraintInspectorWidget::on_processRemoved>(this);
    model().processes.orderChanged.connect<ConstraintInspectorWidget, &ConstraintInspectorWidget::on_orderChanged>(this);
    model().racks.added.connect<ProcessViewTabWidget, &ProcessViewTabWidget::on_rackCreated>(m_viewTabPage); // todo maybe not working ...
    model().racks.removed.connect<ProcessViewTabWidget, &ProcessViewTabWidget::on_rackRemoved>(m_viewTabPage);

    con(model(), &ConstraintModel::viewModelCreated,
        this, &ConstraintInspectorWidget::on_constraintViewModelCreated);
    con(model(), &ConstraintModel::viewModelRemoved,
        this, &ConstraintInspectorWidget::on_constraintViewModelRemoved);


    updateDisplayedValues();

    m_delegate->addWidgets_post(m_properties, this);

    // Display data
    updateAreaLayout(m_properties);
}

ConstraintInspectorWidget::~ConstraintInspectorWidget()
{

}

const ConstraintModel& ConstraintInspectorWidget::model() const
{
    return m_model;
}

QString ConstraintInspectorWidget::tabName()
{
    return tr("Constraint");
}
void ConstraintInspectorWidget::updateDisplayedValues()
{
    // Cleanup the widgets

    m_processesTabPage->updateDisplayedValues();
    m_viewTabPage->updateDisplayedValues();

    m_delegate->updateElements();
}

QWidget* ConstraintInspectorWidget::makeStatesWidget(Scenario::ScenarioModel* scenar)
{
    QWidget* eventWid = new QWidget{this};
    auto eventLay = new iscore::MarginLess<QHBoxLayout>{eventWid};

    eventLay->addStretch(1);
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
    eventLay->addStretch(1);

    return eventWid;
}

void ConstraintInspectorWidget::on_processCreated(
        const Process::ProcessModel&)
{
    // OPTIMIZEME
    m_processesTabPage->updateDisplayedValues();
}

void ConstraintInspectorWidget::on_processRemoved(
        const Process::ProcessModel&)
{
    // OPTIMIZEME
    m_processesTabPage->updateDisplayedValues();
}

void ConstraintInspectorWidget::on_orderChanged()
{
    // OPTIMIZEME
    m_processesTabPage->updateDisplayedValues();
}

void ConstraintInspectorWidget::on_constraintViewModelCreated(const ConstraintViewModel&)
{
    // OPTIMIZEME
    m_viewTabPage->rackWidget()->viewModelsChanged();
}

void ConstraintInspectorWidget::on_constraintViewModelRemoved(const QObject*)
{
    // OPTIMIZEME
    m_viewTabPage->rackWidget()->viewModelsChanged();
}
}
