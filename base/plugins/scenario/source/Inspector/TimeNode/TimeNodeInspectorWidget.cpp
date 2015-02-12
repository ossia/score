#include "TimeNodeInspectorWidget.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"

#include <InspectorInterface/InspectorSectionWidget.hpp>
#include "Inspector/MetadataWidget.hpp"

#include "Commands/Metadata/ChangeElementLabel.hpp"
#include "Commands/Metadata/ChangeElementName.hpp"
#include "Commands/Metadata/ChangeElementComments.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QApplication>
#include <QCompleter>

using namespace Scenario::Command;


TimeNodeInspectorWidget::TimeNodeInspectorWidget (TimeNodeModel* object, QWidget* parent) :
	InspectorWidgetBase{nullptr},
	m_timeNodeModel{object}
{
	setObjectName ("TimeNodeInspectorWidget");
    setInspectedObject(object);
	setParent(parent);

    // default date
    QWidget* dateWid = new QWidget{this};
    QHBoxLayout* dateLay = new QHBoxLayout{dateWid};

    auto dateTitle = new QLabel{"default date : "};
	m_date = new QLabel{QString::number(object->date().msec())};

    dateLay->addWidget(dateTitle);
    dateLay->addWidget(m_date);

    // Events ids list
    m_eventList = new InspectorSectionWidget{"Events", this};

    m_properties.push_back(dateWid);
    m_properties.push_back(m_eventList);

    updateSectionsView (areaLayout(), m_properties);
	areaLayout()->addStretch();

	// display data
	updateDisplayedValues (object);

    // metadata
    m_metadata = new MetadataWidget(&object->metadata, this);
    m_metadata->setType("TimeNode");
    addHeader(m_metadata);

    connect(object, &TimeNodeModel::dateChanged,
            this,   &TimeNodeInspectorWidget::updateInspector);

    connect(m_metadata,     &MetadataWidget::scriptingNameChanged,
            this,           &TimeNodeInspectorWidget::on_scriptingNameChanged);

    connect(m_metadata,     &MetadataWidget::labelChanged,
            this,           &TimeNodeInspectorWidget::on_labelChanged);

    connect(m_metadata,     &MetadataWidget::commentsChanged,
            this,           &TimeNodeInspectorWidget::on_commentsChanged);
}


void TimeNodeInspectorWidget::updateDisplayedValues (TimeNodeModel* timeNode)
{
	// Cleanup
    for(auto& elt : m_events)
	{
		delete elt;
	}
    m_events.clear();

    // DEMO
    if (timeNode)
	{
//        setName (timeNode->metadata.name() );
//		setColor (timeNode->metadata.color() );
//		setComments (timeNode->metadata.comment() );

		m_date->setText(QString::number(m_timeNodeModel->date().msec()));
        for(id_type<EventModel> event : timeNode->events())
        {
            auto label = new QLabel{QString::number((*event.val())), this};

            m_events.push_back(label);
            m_eventList->addContent(label);
        }

//        setInspectedObject (timeNode);
//		changeLabelType ("TimeNode");

    }
}


void TimeNodeInspectorWidget::updateInspector()
{
    updateDisplayedValues(m_timeNodeModel);
}

void TimeNodeInspectorWidget::on_scriptingNameChanged(QString newName)
{
    auto cmd = new ChangeElementName<TimeNodeModel>( ObjectPath::pathFromObject(inspectedObject()),
                newName);

    submitCommand(cmd);
}

void TimeNodeInspectorWidget::on_labelChanged(QString newLabel)
{
    auto cmd = new ChangeElementLabel<TimeNodeModel>( ObjectPath::pathFromObject(inspectedObject()),
                newLabel);

    submitCommand(cmd);
}

void TimeNodeInspectorWidget::on_commentsChanged(QString newComments)
{
    /*
    auto cmd = new ChangeElementComments<TimeNodeModel>( ObjectPath::pathFromObject(inspectedObject()),
                newComments);

    submitCommand(cmd);
    */
}
