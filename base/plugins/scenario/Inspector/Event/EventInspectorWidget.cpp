#include "EventInspectorWidget.hpp"
#include <interface/inspector/InspectorSectionWidget.hpp>

#include <QLabel>
#include <QLineEdit>
//#include <QSpinBox>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
//#include <QToolButton>
#include <QPushButton>
//#include <QDebug>
#include <QScrollArea>

#include <Event/EventModel.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>

#include <Event/State/State.hpp>
#include <Commands/Event/AddStateToEventCommand.hpp>

EventInspectorWidget::EventInspectorWidget (EventModel* object, QWidget* parent) :
	InspectorWidgetBase{nullptr},
	m_eventModel{object}
{
	setObjectName ("EventInspectorWidget");
	setParent(parent);

	QWidget* addressesWidget = new QWidget{this};
	auto dispLayout = new QVBoxLayout{addressesWidget};
	addressesWidget->setLayout(dispLayout);

	QWidget* addAddressWidget = new QWidget{this};
	auto addLayout = new QHBoxLayout{addAddressWidget};
	m_addressLineEdit = new QLineEdit{addAddressWidget};
	auto ok_button = new QPushButton{"Add", addAddressWidget};
	connect(ok_button, &QPushButton::clicked,
			this,	   &EventInspectorWidget::on_addAddressClicked);
	addLayout->addWidget(m_addressLineEdit);
	addLayout->addWidget(ok_button);


	m_properties.push_back(addressesWidget);
	m_properties.push_back(addAddressWidget);

	updateSectionsView (areaLayout(), &m_properties);
	areaLayout()->addStretch();

	// display data
	updateDisplayedValues (object);
}

void EventInspectorWidget::addAddress(const QString& addr)
{
	auto lbl = new QLabel{addr, this};

	m_addresses.push_back(lbl);
	m_properties[0]->layout()->addWidget(lbl);
}

void EventInspectorWidget::updateDisplayedValues (EventModel* event)
{
	// Cleanup
	for(auto& elt : m_addresses)
	{
		delete elt;
	}
	m_addresses.clear();

	// DEMO
	if (event)
	{
//		setName (event->name() );
//		setColor (event->color() );
//		setComments (event->comment() );
		setInspectedObject (event);
		changeLabelType ("Event");


		for(State* state : event->states())
		{
			for(auto& msg : state->messages())
			{
				addAddress(msg);
			}
		}
	}
}

void EventInspectorWidget::on_addAddressClicked()
{
	auto txt = m_addressLineEdit->text();
	auto cmd = new AddStateToEventCommand(ObjectPath::pathFromObject("BaseIntervalModel", m_eventModel),
										  txt);

	emit submitCommand(cmd);
}

void EventInspectorWidget::updateMessages()
{
	updateDisplayedValues(m_eventModel);
}
