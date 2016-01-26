#include <Inspector/Separator.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Inspector/Expression/ExpressionEditorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Inspector/State/StateInspectorWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <Inspector/InspectorSectionWidget.hpp>

#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QColor>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>

#include <QString>
#include <QWidget>
#include <algorithm>

#include "EventInspectorWidget.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <State/Expression.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>

namespace Scenario
{
EventInspectorWidget::EventInspectorWidget(
        const EventModel& object,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    InspectorWidgetBase {object, doc, parent},
    m_model {object}
{
    setObjectName("EventInspectorWidget");
    setParent(parent);

    auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
    ISCORE_ASSERT(scenar);

    con(m_model, &EventModel::statesChanged,
            this,    &EventInspectorWidget::updateDisplayedValues);
    con(m_model, &EventModel::dateChanged,
            this,   &EventInspectorWidget::modelDateChanged);

    ////// HEADER
    // metadata
    m_metadata = new MetadataWidget{&m_model.metadata, commandDispatcher(), &m_model, this};
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
    auto& tn = scenar->timeNode(m_model.timeNode());
    m_triggerWidg = new TriggerInspectorWidget{
                    doc.app.components.factory<Command::TriggerCommandFactoryList>(),
                    tn,
                    this};

    auto trigSection = new Inspector::InspectorSectionWidget{"Trigger", false, this};
    trigSection->expand();
    trigSection->addContent(m_triggerWidg);
    m_properties.push_back(trigSection);

    // Separator
    m_properties.push_back(new Inspector::Separator {this});

    // Condition
    m_exprEditor = new ExpressionEditorWidget{this};
    connect(m_exprEditor, &ExpressionEditorWidget::editingFinished,
            this, &EventInspectorWidget::on_conditionChanged);
    con(m_model, &EventModel::conditionChanged,
        m_exprEditor, &ExpressionEditorWidget::setExpression);

    auto condSection = new Inspector::InspectorSectionWidget{"Condition", false, this};
    condSection->expand();
    condSection->addContent(m_exprEditor);
    m_properties.push_back(condSection);

    // State
    m_properties.push_back(new Inspector::Separator {this});
    m_statesWidget = new QWidget{this};
    auto dispLayout = new QVBoxLayout{m_statesWidget};
    m_statesWidget->setLayout(dispLayout);

    m_properties.push_back(new QLabel{"States"});
    m_properties.push_back(m_statesWidget);

    // Separator
    m_properties.push_back(new Inspector::Separator {this});

    // Plugins (TODO factorize with ConstraintInspectorWidget)
    for(auto& plugdata : m_model.pluginModelList.list())
    {
        for(auto plugin : doc.pluginModels())
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

QString EventInspectorWidget::tabName()
{
    return tr("Event");
}

void EventInspectorWidget::addState(const StateModel& state)
{
    auto sw = new StateInspectorWidget{state, context(), this};
    sw->hide(); // TODO UGLY : we create a state (inspectorbase) just to extract the section ...
    auto& section = sw->stateSection();
    section.showDeleteButton(true);

    m_states.push_back(sw);
    m_statesWidget->layout()->addWidget(&section);
    m_states.push_back(&section);
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

    auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
    ISCORE_ASSERT(scenar);
    for(const auto& state : m_model.states())
    {
        addState(scenar->state(state));
    }

    m_exprEditor->setExpression(m_model.condition());
    auto& tn = scenar->timeNode(m_model.timeNode());
    m_triggerWidg->updateExpression(tn.trigger()->expression());
}


using namespace iscore::IDocument;
using namespace Scenario;

void EventInspectorWidget::on_conditionChanged()
{
    auto cond = m_exprEditor->expression();

    if(cond != m_model.condition())
    {
        auto cmd = new Scenario::Command::SetCondition{path(m_model), std::move(cond)};
        emit commandDispatcher()->submitCommand(cmd);
    }
}

void EventInspectorWidget::modelDateChanged()
{
    m_date->setText(m_model.date().toString());
}
}
