#include "EventInspectorWidget.hpp"

#include "Document/Event/EventModel.hpp"

#include "Document/Event/State/State.hpp"
#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Event/SetCondition.hpp"

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

#include "base/plugins/device_explorer/DeviceInterface/DeviceCompleter.hpp"
#include "base/plugins/device_explorer/DeviceInterface/DeviceExplorerInterface.hpp"
using namespace Scenario;

EventInspectorWidget::EventInspectorWidget (EventModel* object, QWidget* parent) :
	InspectorWidgetBase{nullptr},
	m_eventModel{object}
{
	setObjectName ("EventInspectorWidget");
	setParent(parent);

	m_conditionWidget = new QLineEdit{this};
	connect(m_conditionWidget, SIGNAL(editingFinished()),
			this,			 SLOT(on_conditionChanged()));
    // todo : attention, ordre de m_properties utilisé (dans addAddress() !! faudrait changer ...
	m_properties.push_back(new QLabel{"Condition"});
	m_properties.push_back(m_conditionWidget);

	QWidget* addressesWidget = new QWidget{this};
	auto dispLayout = new QVBoxLayout{addressesWidget};
	addressesWidget->setLayout(dispLayout);

	QWidget* addAddressWidget = new QWidget{this};
	auto addLayout = new QHBoxLayout{addAddressWidget};

	m_addressLineEdit = new QLineEdit{addAddressWidget};

	auto deviceexplorer = DeviceExplorer::getModel(object);
	if(deviceexplorer)
	{
		auto completer = new DeviceCompleter{deviceexplorer, this};
		m_addressLineEdit->setCompleter(completer);
	}


	auto ok_button = new QPushButton{"Add", addAddressWidget};
	connect(ok_button, &QPushButton::clicked,
			this,	   &EventInspectorWidget::on_addAddressClicked);
	addLayout->addWidget(m_addressLineEdit);
	addLayout->addWidget(ok_button);

	m_properties.push_back(new QLabel{"States"});
	m_properties.push_back(addressesWidget);
	m_properties.push_back(addAddressWidget);

	updateSectionsView (areaLayout(), m_properties);
	areaLayout()->addStretch();

	// display data
	updateDisplayedValues (object);
}

void EventInspectorWidget::addAddress(const QString& addr)
{
	auto lbl = new QLabel{addr, this};

	m_addresses.push_back(lbl);
    m_properties[3]->layout()->addWidget(lbl);
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
        setName (event->name() );
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

        m_conditionWidget->setText(event->condition());
	}
}

void EventInspectorWidget::on_addAddressClicked()
{
	auto txt = m_addressLineEdit->text();
	auto cmd = new Command::AddStateToEvent{
					ObjectPath::pathFromObject("BaseConstraintModel",
											   m_eventModel),
					txt};

	emit submitCommand(cmd);
	m_addressLineEdit->clear();
}

void EventInspectorWidget::on_conditionChanged()
{
	auto txt = m_conditionWidget->text();
	if(txt == m_eventModel->condition()) return;

	auto cmd = new Command::SetCondition{
					ObjectPath::pathFromObject("BaseConstraintModel",
											   m_eventModel),
					txt};

	emit submitCommand(cmd);

}

void EventInspectorWidget::updateMessages()
{
	updateDisplayedValues(m_eventModel);
}
