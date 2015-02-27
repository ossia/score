#include "TimeNodeInspectorWidget.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"

#include <InspectorInterface/InspectorSectionWidget.hpp>
#include "Inspector/MetadataWidget.hpp"
#include "Inspector/Event/EventWidgets/EventShortcut.hpp"

#include "Commands/TimeNode/SplitTimeNode.hpp"

#include "core/interface/document/DocumentInterface.hpp"

#include <iostream>
#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QApplication>
#include <QCompleter>
#include <QCheckBox>

using namespace Scenario::Command;


TimeNodeInspectorWidget::TimeNodeInspectorWidget(TimeNodeModel* object, QWidget* parent) :
    InspectorWidgetBase {nullptr},
    m_model {object}
{
    setObjectName("TimeNodeInspectorWidget");
    setInspectedObject(object);
    setParent(parent);

    // default date
    QWidget* dateWid = new QWidget{this};
    QHBoxLayout* dateLay = new QHBoxLayout{dateWid};

    auto dateTitle = new QLabel{"default date : "};
    m_date = new QLabel{QString::number(object->date().msec()) };

    dateLay->addWidget(dateTitle);
    dateLay->addWidget(m_date);

    // Events ids list
    m_eventList = new InspectorSectionWidget{"Events", this};

    m_properties.push_back(dateWid);
    m_properties.push_back(m_eventList);

    updateSectionsView(areaLayout(), m_properties);
    areaLayout()->addStretch();

    // display data
    updateDisplayedValues(object);

    // metadata
    m_metadata = new MetadataWidget{&object->metadata, this};
    m_metadata->setType(TimeNodeModel::prettyName()); // TODO le faire automatiquement avec T::className
    connect(m_metadata, &MetadataWidget::submitCommand,
            this,       &InspectorWidgetBase::submitCommand);
    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    connect(object, &TimeNodeModel::dateChanged,
            this,   &TimeNodeInspectorWidget::updateInspector);

//    connect(m_metadata, &MetadataWidget::inspectPreviousElement,
//           m_model,    &TimeNodeModel::inspectPreviousElement);

    auto splitBtn = new QPushButton{this};
    splitBtn->setText("split timeNode");

    m_eventList->addContent(splitBtn);

    connect(splitBtn,   &QPushButton::clicked,
            this,       &TimeNodeInspectorWidget::on_splitTimeNodeClicked);
}


void TimeNodeInspectorWidget::updateDisplayedValues(TimeNodeModel* timeNode)
{
    // Cleanup
    for(auto& elt : m_events)
    {
        delete elt;
    }

    m_events.clear();

    // DEMO
    if(timeNode)
    {
//        setName (timeNode->metadata.name() );
//		setColor (timeNode->metadata.color() );
//		setComments (timeNode->metadata.comment() );

        m_date->setText(QString::number(m_model->date().msec()));

        for(id_type<EventModel> event : timeNode->events())
        {
            auto eventWid = new EventShortCut(QString::number((*event.val())));

            m_events.push_back(eventWid);
            m_eventList->addContent(eventWid);

            // TODO
//            connect(eventWid,           &EventShortCut::eventSelected,
//                    m_model,    &TimeNodeModel::eventSelected);

        }


//        setInspectedObject (timeNode);
//		changeLabelType ("TimeNode");

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
        auto cmd = new SplitTimeNode(iscore::IDocument::path(inspectedObject()),
                                     eventGroup);

        submitCommand(cmd);

        qDebug() << info;
    }
}
