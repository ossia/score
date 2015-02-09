#include "TimeNodeInspectorWidget.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"

#include <InspectorInterface/InspectorSectionWidget.hpp>

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QApplication>
#include <QCompleter>


TimeNodeInspectorWidget::TimeNodeInspectorWidget (TimeNodeModel* object, QWidget* parent) :
	InspectorWidgetBase{nullptr},
	m_timeNodeModel{object}
{
	setObjectName ("TimeNodeInspectorWidget");
	setParent(parent);

    m_eventList = new InspectorSectionWidget{"Events", this};

    QWidget* dateWid = new QWidget{this};
    QHBoxLayout* dateLay = new QHBoxLayout{dateWid};

    auto dateTitle = new QLabel{"default date : "};
    m_date = new QLabel{QString::number(object->date())};

    dateLay->addWidget(dateTitle);
    dateLay->addWidget(m_date);

    m_properties.push_back(dateWid);
    m_properties.push_back(m_eventList);

    updateSectionsView (areaLayout(), m_properties);
	areaLayout()->addStretch();

	// display data
	updateDisplayedValues (object);

    connect(object, &TimeNodeModel::dateChanged,
            this,   &TimeNodeInspectorWidget::updateInspector);
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
//        setName (timeNode->name() );
//		setColor (timeNode->color() );
//		setComments (timeNode->comment() );

        m_date->setText(QString::number(m_timeNodeModel->date()));
        for(id_type<EventModel> event : timeNode->events())
        {
            auto label = new QLabel{QString::number((*event.val())), this};

            m_events.push_back(label);
            m_eventList->addContent(label);
        }

        setInspectedObject (timeNode);
		changeLabelType ("TimeNode");

    }
}


void TimeNodeInspectorWidget::updateInspector()
{
	updateDisplayedValues(m_timeNodeModel);
}
