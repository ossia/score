#include "EventInspectorWidget.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Scenario/Commands/Event/SetCondition.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/Separator.hpp>
#include <Inspector/InspectorWidgetList.hpp>

#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Inspector/State/StateInspectorWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Widgets/DeviceCompleter.hpp>
#include <Explorer/Widgets/DeviceExplorerMenuButton.hpp>

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFormLayout>

#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/widgets/MarginLess.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

EventInspectorWidget::EventInspectorWidget(
        const EventModel& object,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {object, doc, parent},
    m_model {object}
{
    setObjectName("EventInspectorWidget");
    setParent(parent);

    con(m_model, &EventModel::statesChanged,
            this,    &EventInspectorWidget::updateDisplayedValues);
    con(m_model, &EventModel::dateChanged,
            this,   &EventInspectorWidget::modelDateChanged);

    ////// HEADER
    // metadata
    m_metadata = new MetadataWidget{&m_model.metadata, commandDispatcher(), &m_model, this};
    m_metadata->setType(EventModel::prettyName());
    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    ////// BODY
    /// Information
    auto infoWidg = new QWidget;
    auto infoLay = new iscore::MarginLess<QFormLayout>;
    infoWidg->setLayout(infoLay);

    // timeNode
    auto timeNode = m_model.timeNode();
    if(timeNode)
    {
        auto scenar = m_model.parentScenario();
        auto tnBtn = SelectionButton::make(
                    tr("Parent TimeNode"),
                    &scenar->timeNode(timeNode),
                    selectionDispatcher(),
                    this);

        infoLay->addWidget(tnBtn);
    }

    // date
    auto datewidg = new QWidget;
    auto dateLay = new iscore::MarginLess<QHBoxLayout>;
    datewidg->setLayout(dateLay);
    m_date = new QLabel{(m_model.date().toString())};

    dateLay->addWidget(new QLabel(tr("Default date")));
    dateLay->addWidget(m_date);

    infoLay->addWidget(datewidg);
    m_properties.push_back(infoWidg);

    // Trigger
    auto& tn = m_model.parentScenario()->timeNode(m_model.timeNode());
    m_triggerWidg = new TriggerInspectorWidget{tn, this};
    m_properties.push_back(new QLabel{tr("Trigger")});
    m_properties.push_back(m_triggerWidg);

    // Separator
    m_properties.push_back(new Separator {this});

    // Condition
    m_conditionLineEdit = new QLineEdit{this};
    m_conditionLineEdit->setValidator(&m_validator);

    connect(m_conditionLineEdit, &QLineEdit::editingFinished,
            this, &EventInspectorWidget::on_conditionChanged);
    con(m_model, &EventModel::conditionChanged,
        this, [this] (const iscore::Condition& c) {
        m_conditionLineEdit->setText(c.toString());
    });

    m_properties.push_back(new QLabel{tr("Condition")});
    m_properties.push_back(m_conditionLineEdit);

    // State
    m_properties.push_back(new Separator {this});
    m_statesWidget = new QWidget{this};
    auto dispLayout = new QVBoxLayout{m_statesWidget};
    m_statesWidget->setLayout(dispLayout);

    m_properties.push_back(new QLabel{"States"});
    m_properties.push_back(m_statesWidget);

    // Separator
    m_properties.push_back(new Separator {this});

    // Plugins (TODO factorize with ConstraintInspectorWidget)
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
}

void EventInspectorWidget::addState(const StateModel& state)
{
    auto sw = new StateInspectorWidget{state, doc(), this};

    m_states.push_back(sw);
    m_statesWidget->layout()->addWidget(sw);
}

void EventInspectorWidget::removeState(const StateModel& state)
{
    // this is not connected
    ISCORE_TODO;
}

void EventInspectorWidget::focusState(const StateModel* state)
{
    ISCORE_TODO;
}

void EventInspectorWidget::updateDisplayedValues()
{
    // Cleanup
    for(auto& elt : m_states)
    {
        delete elt;
    }

    m_states.clear();
    m_date->clear();

    m_date->setText(m_model.date().toString());

    const auto& scenar = m_model.parentScenario();
    for(const auto& state : m_model.states())
    {
        addState(scenar->state(state));
    }

    m_conditionLineEdit->setText(m_model.condition().toString());
    auto& tn = m_model.parentScenario()->timeNode(m_model.timeNode());
    m_triggerWidg->updateExpression(tn.trigger()->expression().toString());
}


using namespace iscore::IDocument;
using namespace Scenario;

void EventInspectorWidget::on_conditionChanged()
{
    auto cond = m_validator.get();

    if(*cond != m_model.condition())
    {
        auto cmd = new Scenario::Command::SetCondition{path(m_model), std::move(*cond)};
        emit commandDispatcher()->submitCommand(cmd);
    }
}

void EventInspectorWidget::modelDateChanged()
{
    m_date->setText(m_model.date().toString());
}
