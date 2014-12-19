#include "ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Commands/Constraint/AddProcessToConstraintCommand.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <InspectorInterface/InspectorSectionWidget.hpp>

#include <source/Control/ProcessList.hpp> // Bad include. Put this in ProcessInterface

#include <tools/ObjectPath.hpp>

#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QToolButton>
#include <QPushButton>
#include <QtWidgets/QInputDialog>
#include <QApplication>

ConstraintInspectorWidget::ConstraintInspectorWidget (ConstraintModel* object, QWidget* parent) :
	InspectorWidgetBase (parent)
{
	setObjectName ("Constraint");

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

	addAutomLayout->addWidget (addAutomButton);
	addAutomLayout->addWidget (addAutom);
	addAutomWidget->setLayout (addAutomLayout);

	connect (addAutomButton, &QPushButton::pressed,
			 [=] ()
	{
		bool ok = false;
		auto process_list = ProcessList::getProcessesName();
		auto process_name = QInputDialog::getItem(this,
												  tr("Choose a process"),
												  tr("Choose a process"),
												  process_list,
												  0,
												  false,
												  &ok);

		if(ok)
			createProcess(process_name);
	} );
	connect (addAutom, SIGNAL (released() ), this, SLOT (addAutomation() ) );

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
	automSection->renameSection ("Processes");
	automSection->setObjectName ("Processes");


	InspectorSectionWidget* startSection = new InspectorSectionWidget (this);
	startSection->renameSection ("Start");
	startSection->addContent (startWidget);
	startSection->setObjectName ("Start");

	InspectorSectionWidget* endSection = new InspectorSectionWidget (this);
	endSection->renameSection ("End");
	endSection->addContent (endWidget);
	endSection->setObjectName ("End");

	_properties.push_back (automSection);
	_properties.push_back (addAutomWidget);
	_properties.push_back (line);
	_properties.push_back (startSection);
	_properties.push_back (endSection);

	updateSectionsView (areaLayout(), _properties);
	areaLayout()->addStretch();

	// display data
	updateDisplayedValues (object);
}

void ConstraintInspectorWidget::addAutomation (QString address)
{
	InspectorSectionWidget* autom = findChild<InspectorSectionWidget*> ("Automations");

	if (autom != nullptr)
	{
		InspectorSectionWidget* newAutomation = new InspectorSectionWidget (address);

		// the last automation created is by default in edit name mode
		if (!_automations.empty() )
		{
			static_cast<InspectorSectionWidget*> (_automations.back() )->nameEditDisable();
		}

		_automations.push_back (newAutomation);
		autom->addContent (newAutomation);
		newAutomation->nameEditEnable();
	}
}

void ConstraintInspectorWidget::updateDisplayedValues (ConstraintModel* constraint)
{
	// TODO clear everything.
	// DEMO
	if (constraint != nullptr)
	{
		m_currentConstraint = constraint;
		setName (constraint->name() );
		setColor (constraint->color() );
		setComments (constraint->comment() );
		setInspectedObject (constraint);
		changeLabelType ("Constraint");
		/*
		_startForm->addRow ("/first/message", new QLineEdit);
		_endForm->addRow ("/Last/message", new QLineEdit );
		*/
		for(ProcessSharedModelInterface* process : constraint->processes())
		{
			displayProcess(process);
			if (!_automations.empty() )
			{
				static_cast<InspectorSectionWidget*> (_automations.back())->nameEditDisable();
			}
		}
	}
	else
	{
		m_currentConstraint = nullptr;
	}
}

void ConstraintInspectorWidget::reorderAutomations()
{
	//	new ReorderWidget (_automations);
}

void ConstraintInspectorWidget::createProcess(QString processName)
{
	qDebug() << "Adding process" << processName;

	auto cmd = new AddProcessToConstraintCommand(
						ObjectPath::pathFromObject("BaseConstraintModel",
												   m_currentConstraint),
						processName);
	emit submitCommand(cmd);

	updateDisplayedValues(m_currentConstraint);

}

void ConstraintInspectorWidget::displayProcess(ProcessSharedModelInterface* process)
{
	InspectorSectionWidget* proc = findChild<InspectorSectionWidget*> ("Processes");

	if (proc != nullptr)
	{
		InspectorSectionWidget* newProc = new InspectorSectionWidget (process->processName());

		// the last automation created is by default in edit name mode
		if (!_automations.empty() )
		{
			static_cast<InspectorSectionWidget*> (_automations.back() )->nameEditDisable();
		}

		_automations.push_back (newProc);
		proc->addContent (newProc);
		//newAutomation->nameEditEnable();
	}
}
