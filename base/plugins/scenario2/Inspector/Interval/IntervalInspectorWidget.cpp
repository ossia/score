#include "IntervalInspectorWidget.hpp"
#include <interface/inspector/InspectorSectionWidget.hpp>

#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QToolButton>
#include <QPushButton>
#include <QDebug>
#include <QScrollArea>
#include <Interval/IntervalModel.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>

IntervalInspectorWidget::IntervalInspectorWidget (IntervalModel* object, QWidget* parent) :
	InspectorWidgetBase (parent)
{
	setObjectName ("Interval");
	_properties = new std::vector<QWidget*>;
	_automations = new std::vector<QWidget*>;

// Add Automation Section
	QWidget* addAutomWidget = new QWidget;
	QHBoxLayout* addAutomLayout = new QHBoxLayout;
	QPushButton* addAutom = new QPushButton ("Add Automation");
	addAutom->setStyleSheet (QString ("text-align : left;") );
	addAutom->setFlat (true);

	// addAutom button
	QToolButton* addAutomButton = new QToolButton;
	addAutomButton->setText ("+");
	addAutomButton->setObjectName ("addAutom");

	/*   // reorderButton
	   QToolButton *reorderButton = new QToolButton;
	   reorderButton->setText("*");
	   reorderButton->setObjectName("reorderButton");
	*/
	addAutomLayout->addWidget (addAutomButton);
	addAutomLayout->addWidget (addAutom);
//    addAutomLayout->addWidget(reorderButton);
	addAutomWidget->setLayout (addAutomLayout);

	connect (addAutomButton, SIGNAL (released() ), this, SLOT (addAutomation() ) );
	connect (addAutom, SIGNAL (released() ), this, SLOT (addAutomation() ) );
//    connect(reorderButton, SIGNAL(released()), this, SLOT(reorderAutomations()));


	// line
	QFrame* line = new QFrame();
	line->setFrameShape (QFrame::HLine);
	line->setLineWidth (2);

// Start state
	QWidget* startWidget = new QWidget;
	_startForm = new QFormLayout (startWidget);

// End State
	QWidget* endWidget = new QWidget;
	_endForm = new QFormLayout (endWidget);


	// Sections

	InspectorSectionWidget* automSection = new InspectorSectionWidget (this);
	automSection->renameSection ("Automations");
	automSection->setObjectName ("Automations");


	InspectorSectionWidget* startSection = new InspectorSectionWidget (this);
	startSection->renameSection ("Start");
	startSection->addContent (startWidget);
	startSection->setObjectName ("Start");

	InspectorSectionWidget* endSection = new InspectorSectionWidget (this);
	endSection->renameSection ("End");
	endSection->addContent (endWidget);
	endSection->setObjectName ("End");

	_properties->push_back (automSection);
	_properties->push_back (addAutomWidget);
	_properties->push_back (line);
	_properties->push_back (startSection);
	_properties->push_back (endSection);

	updateSectionsView (areaLayout(), _properties);
	areaLayout()->addStretch();

	// display data
	updateDisplayedValues (object);
}

void IntervalInspectorWidget::addAutomation (QString address)
{
	InspectorSectionWidget* autom = findChild<InspectorSectionWidget*> ("Automations");

	if (autom != nullptr)
	{
		InspectorSectionWidget* newAutomation = new InspectorSectionWidget (address);

		// the last automation created is by default in edit name mode
		if (!_automations->empty() )
		{
			static_cast<InspectorSectionWidget*> (_automations->back() )->nameEditDisable();
		}

		_automations->push_back (newAutomation);
		autom->addContent (newAutomation);
		newAutomation->nameEditEnable();
	}
}

void IntervalInspectorWidget::updateDisplayedValues (IntervalModel* interval)
{
	// DEMO
	if (interval != nullptr)
	{
		setName (interval->name() );
		setColor (interval->color() );
		setComments (interval->comment() );
		setInspectedObject (interval);
		changeLabelType ("TimeBox");
		_startForm->addRow ("/first/message", new QLineEdit);
		_endForm->addRow ("/Last/message", new QLineEdit );

		for(iscore::ProcessSharedModelInterface* process : interval->processes())
		{
			if(process->processName() == "Automation")
			{
				addAutomation("/test/lol");
				// Todo : caster en AutomationProcessModel et l'afficher, qui n'existe pas encore ^_^.
			}

			if (!_automations->empty() )
			{
				static_cast<InspectorSectionWidget*> (_automations->back() )->nameEditDisable();
			}
		}
	}
}

void IntervalInspectorWidget::reorderAutomations()
{
//	new ReorderWidget (_automations);
}
