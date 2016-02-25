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
#include <State/Expression.hpp>

#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QColor>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>
#include <QMenu>
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
    QWidget{parent},
    m_model {object},
    m_context{doc},
    m_commandDispatcher{new CommandDispatcher<>{doc.commandStack}},
    m_selectionDispatcher{new iscore::SelectionDispatcher{doc.selectionStack}}
{
    setObjectName("EventInspectorWidget");
    setParent(parent);

    auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
    ISCORE_ASSERT(scenar);

    con(m_model, &EventModel::statesChanged,
            this,    &EventInspectorWidget::updateDisplayedValues);

    ////// HEADER
    // metadata
    m_metadata = new MetadataWidget{&m_model.metadata, commandDispatcher(), &m_model, this};
    m_metadata->setupConnections(m_model);

    m_properties.push_back(m_metadata);

    ////// BODY
    /// Information
    auto infoWidg = new QWidget;
    auto infoLay = new iscore::MarginLess<QFormLayout>{infoWidg};

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

    // Separator
    m_properties.push_back(new Inspector::HSeparator {this});

    // Condition
    m_exprEditor = new ExpressionEditorWidget{m_context, this};
    connect(m_exprEditor, &ExpressionEditorWidget::editingFinished,
            this, &EventInspectorWidget::on_conditionChanged);
    con(m_model, &EventModel::conditionChanged,
        m_exprEditor, &ExpressionEditorWidget::setExpression);

    auto condSection = new Inspector::InspectorSectionWidget{"Condition", false, this};
    condSection->addContent(m_exprEditor);
    m_properties.push_back(condSection);

    condSection->expand(!m_model.condition().toString().isEmpty());

    // State
    m_properties.push_back(new Inspector::HSeparator {this});
    m_properties.push_back(new QLabel{"States"});

    m_statesWidget = new QWidget{this};
    auto dispLayout = new QVBoxLayout{m_statesWidget};
    m_statesWidget->setLayout(dispLayout);
    dispLayout->setSizeConstraint(QLayout::SetMinimumSize);


    m_properties.push_back(m_statesWidget);

    // Separator
    m_properties.push_back(new Inspector::HSeparator {this});

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
//    updateAreaLayout(m_properties);

    auto lay = new iscore::MarginLess<QVBoxLayout>{this};
    for(auto w : m_properties)
        lay->addWidget(w);
}

void EventInspectorWidget::addState(const StateModel& state)
{
    auto sw = new StateInspectorWidget{state, m_context, this};
    sw->hide(); // TODO UGLY : we create a state (inspectorbase) just to extract the section ...
    auto& section = sw->stateSection();
    section.showMenu(true);
    auto split = section.menu()->addAction(tr("Put in new Event"));
    connect(split, &QAction::triggered,
            sw, &StateInspectorWidget::splitEvent, Qt::QueuedConnection);

    m_states.push_back(sw);
    m_statesWidget->layout()->addWidget(&section);
    m_states.push_back(&section);
    section.expand(false);

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

    auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
    ISCORE_ASSERT(scenar);
    for(const auto& state : m_model.states())
    {
        addState(scenar->state(state));
    }

    m_exprEditor->setExpression(m_model.condition());
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

}
