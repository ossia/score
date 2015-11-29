#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Commands/TimeNode/SplitTimeNode.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Inspector/Event/EventWidgets/EventShortcut.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <boost/optional/optional.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentContext.hpp>
#include <qboxlayout.h>
#include <qcolor.h>
#include <qlabel.h>
#include <qobjectdefs.h>
#include <qpushbutton.h>
#include <qstring.h>
#include <qvector.h>
#include <qwidget.h>
#include <algorithm>

#include "Inspector/InspectorWidgetBase.hpp"
#include "Process/TimeValue.hpp"
#include "Scenario/Process/ScenarioInterface.hpp"
#include "TimeNodeInspectorWidget.hpp"
#include "iscore/command/Dispatchers/CommandDispatcher.hpp"
#include "iscore/plugins/customfactory/StringFactoryKey.hpp"
#include "iscore/selection/Selection.hpp"
#include "iscore/selection/SelectionDispatcher.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/ModelPath.hpp"
#include "iscore/tools/ModelPathSerialization.hpp"
#include "iscore/tools/SettableIdentifier.hpp"
#include "iscore/tools/Todo.hpp"

using namespace Scenario::Command;


TimeNodeInspectorWidget::TimeNodeInspectorWidget(
        const TimeNodeModel& object,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {object, doc, parent},
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
                 doc.context().app.components.factory<TriggerCommandFactoryList>(),
                 m_model, this};


    // Events ids list
    m_eventList = new InspectorSectionWidget{"Events", false, this};

    m_properties.push_back(dateWid);
    m_properties.push_back(new QLabel{tr("Trigger")});

    m_properties.push_back(m_trigwidg);
    m_properties.push_back(m_eventList);

    updateAreaLayout(m_properties);

    // display data
    updateDisplayedValues();

    // metadata
    m_metadata = new MetadataWidget{&m_model.metadata, commandDispatcher(), &m_model, this};
    m_metadata->setType(TimeNodeModel::description());

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    con(m_model, &TimeNodeModel::dateChanged,
        this,   &TimeNodeInspectorWidget::updateDisplayedValues);

    auto splitBtn = new QPushButton{this};
    splitBtn->setText("split timeNode");

    m_eventList->addContent(splitBtn);

    connect(splitBtn,   &QPushButton::clicked,
            this,       &TimeNodeInspectorWidget::on_splitTimeNodeClicked);
}


void TimeNodeInspectorWidget::updateDisplayedValues()
{
    // Cleanup
    // OPTIMIZEME
    for(auto& elt : m_events)
    {
        delete elt;
    }

    m_events.clear();

    m_date->setText(m_model.date().toString());

    for(const auto& event : m_model.events())
    {
        auto scenar = m_model.parentScenario();
        EventModel* evModel = &scenar->event(event);

        auto eventWid = new EventShortCut(QString::number((*event.val())));

        m_events.push_back(eventWid);
        m_eventList->addContent(eventWid);

        connect(eventWid, &EventShortCut::eventSelected,
                [=]()
        {
            selectionDispatcher().setAndCommit(Selection{evModel});
        });
    }

    m_trigwidg->updateExpression(m_model.trigger()->expression());
}

void TimeNodeInspectorWidget::on_splitTimeNodeClicked()
{
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
        auto cmd = new SplitTimeNode{m_model,
                                     eventGroup};

        commandDispatcher()->submitCommand(cmd);
    }

    updateDisplayedValues();
}
