#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Commands/TimeNode/SplitTimeNode.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Inspector/Event/EventInspectorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <boost/optional/optional.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <QBoxLayout>
#include <QColor>
#include <QLabel>

#include <QPushButton>
#include <QString>
#include <QVector>
#include <QWidget>
#include <algorithm>

#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include "TimeNodeInspectorWidget.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/widgets/MarginLess.hpp>

namespace Scenario
{
TimeNodeInspectorWidget::TimeNodeInspectorWidget(
        const TimeNodeModel& object,
        const iscore::DocumentContext& ctx,
        QWidget* parent) :
    InspectorWidgetBase {object, ctx, parent},
    m_model {object}
{
    setObjectName("TimeNodeInspectorWidget");
    setParent(parent);

    // default date
    QWidget* dateWid = new QWidget{this};
    QHBoxLayout* dateLay = new QHBoxLayout{dateWid};

    auto dateTitle = new QLabel{"Default date"};
    m_date = new QLabel{m_model.date().toString() };

    dateLay->addWidget(dateTitle);
    dateLay->addWidget(m_date);

    // Trigger
    m_trigwidg = new TriggerInspectorWidget{
                 ctx,
                 ctx.app.components.factory<Command::TriggerCommandFactoryList>(),
                 m_model, this};


    // Events
    m_events = new QWidget{this};
    auto evLay = new iscore::MarginLess<QVBoxLayout>{m_events};
    evLay->setSizeConstraint(QLayout::SetMinimumSize);

    m_properties.push_back(dateWid);
    m_properties.push_back(new QLabel{tr("Trigger")});

    m_properties.push_back(m_trigwidg);
    m_properties.push_back(new QLabel{tr("Events")});
    m_properties.push_back(m_events);

    updateAreaLayout(m_properties);

    // display data
    updateDisplayedValues();

    // metadata
    m_metadata = new MetadataWidget{&m_model.metadata, commandDispatcher(), &m_model, this};

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    con(m_model, &TimeNodeModel::dateChanged,
        this,   &TimeNodeInspectorWidget::updateDisplayedValues);

/*    auto splitBtn = new QPushButton{this};
    splitBtn->setText("split timeNode");

    m_eventList->addContent(splitBtn);

    connect(splitBtn,   &QPushButton::clicked,
            this,       &TimeNodeInspectorWidget::on_splitTimeNodeClicked);
            */
}

void TimeNodeInspectorWidget::addEvent(const EventModel& event)
{
    auto evSection = new Inspector::InspectorSectionWidget{event.metadata.name(), false, this};
    auto ew = new EventInspectorWidget{event, context(), evSection};
    evSection->addContent(ew);
    m_eventList.push_back(evSection);
    if(!event.selection.get())
    {
        evSection->expand();
    }
    m_properties.push_back(evSection);
    m_events->layout()->addWidget(evSection);
}

void TimeNodeInspectorWidget::removeEvent(const EventModel& event)
{
    ISCORE_TODO;
}

QString TimeNodeInspectorWidget::tabName()
{
    return tr("TimeNode");
}


void TimeNodeInspectorWidget::updateDisplayedValues()
{
    // Cleanup
    // OPTIMIZEME
    for(auto& elt : m_eventList)
    {
        m_properties.remove(elt);
        delete elt;
    }
    m_eventList.clear();

    m_date->setText(m_model.date().toString());

    for(const auto& event : m_model.events())
    {
        auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
        ISCORE_ASSERT(scenar);
        auto& evModel = scenar->event(event);
        addEvent(evModel);
    }

    m_trigwidg->updateExpression(m_model.trigger()->expression());
}

void TimeNodeInspectorWidget::on_splitTimeNodeClicked()
{
    /*
    QVector<Id<EventModel> > eventGroup;

    for(const auto& ev : m_events)
    {
        if(ev->isChecked())
        {
            eventGroup.push_back( Id<EventModel>(ev->eventName().toInt()));
        }
    }

    if (eventGroup.size() < int(m_events.size()))
    {
        auto cmd = new Command::SplitTimeNode{m_model,
                                     eventGroup};

        commandDispatcher()->submitCommand(cmd);
    }

    updateDisplayedValues();
    */
}
}

