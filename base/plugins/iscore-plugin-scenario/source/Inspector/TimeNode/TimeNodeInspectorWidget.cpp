#include "TimeNodeInspectorWidget.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"

#include "Process/ScenarioModel.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include "Inspector/MetadataWidget.hpp"
#include "Inspector/Event/EventWidgets/EventShortcut.hpp"

#include "Commands/TimeNode/SplitTimeNode.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QPushButton>
#include <QCompleter>

using namespace Scenario::Command;


TimeNodeInspectorWidget::TimeNodeInspectorWidget(
        const TimeNodeModel* object,
        QWidget* parent) :
    InspectorWidgetBase {object, parent},
    m_model {object}
{
    setObjectName("TimeNodeInspectorWidget");
    setParent(parent);

    // default date
    QWidget* dateWid = new QWidget{this};
    QHBoxLayout* dateLay = new QHBoxLayout{dateWid};

    auto dateTitle = new QLabel{"Default date"};
    m_date = new QLabel{QString::number(object->date().msec()) };

    dateLay->addWidget(dateTitle);
    dateLay->addWidget(m_date);

    // Events ids list
    m_eventList = new InspectorSectionWidget{"Events", this};

    m_properties.push_back(dateWid);
    m_properties.push_back(m_eventList);

    updateAreaLayout(m_properties);

    // display data
    updateDisplayedValues(object);

    // metadata
    m_metadata = new MetadataWidget{&object->metadata, commandDispatcher(), object, this};
    m_metadata->setType(TimeNodeModel::prettyName());

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    connect(object, &TimeNodeModel::dateChanged,
            this,   &TimeNodeInspectorWidget::updateInspector);

    auto splitBtn = new QPushButton{this};
    splitBtn->setText("split timeNode");

    m_eventList->addContent(splitBtn);

    connect(splitBtn,   &QPushButton::clicked,
            this,       &TimeNodeInspectorWidget::on_splitTimeNodeClicked);
}


void TimeNodeInspectorWidget::updateDisplayedValues(const TimeNodeModel* timeNode)
{
    // Cleanup
    for(auto& elt : m_events)
    {
        delete elt;
    }

    m_events.clear();

   if(timeNode)
    {
        m_date->setText(QString::number(m_model->date().msec()));

        for(const auto& event : timeNode->events())
        {
            auto scenar = timeNode->parentScenario();
            EventModel* evModel = &scenar->event(event);

            auto eventWid = new EventShortCut(QString::number((*event.val())));

            m_events.push_back(eventWid);
            m_eventList->addContent(eventWid);

            connect(eventWid, &EventShortCut::eventSelected,
                    [=]()
            {
                selectionDispatcher()->setAndCommit(Selection{evModel});
            });
        }
    }
}


void TimeNodeInspectorWidget::updateInspector()
{
    updateDisplayedValues(m_model);
}

void TimeNodeInspectorWidget::on_splitTimeNodeClicked()
{
    QString info = "create a timenode with ";

    QVector<id_type<EventModel> > eventGroup;

    for(auto ev : m_events)
    {
        if(ev->isChecked())
        {
            info += ev->eventName(); info += QString(" ") ;
            eventGroup.push_back( id_type<EventModel>(ev->eventName().toInt()));
        }
    }

    if (eventGroup.size() < int(m_events.size()))
    {
        auto cmd = new SplitTimeNode(iscore::IDocument::path(m_model),
                                     eventGroup);

        commandDispatcher()->submitCommand(cmd);

        qDebug() << info;
    }
    updateDisplayedValues(m_model);
}
