#include "AddProcessWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Control/ProcessList.hpp" // TODO Bad include. Put this in ProcessInterface


#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>

SharedProcessWidget::SharedProcessWidget(ConstraintInspectorWidget* parent):
	QWidget{parent}
{
	QHBoxLayout* addAutomLayout = new QHBoxLayout;
	addAutomLayout->setContentsMargins(0, 0, 0 ,0);
	this->setLayout(addAutomLayout);

	// Button
	QToolButton* addAutomButton = new QToolButton;
	addAutomButton->setText ("+");
	addAutomButton->setObjectName ("addAutom");

	// Text
	auto addAutomText = new QLabel("Add Automation");
	addAutomText->setStyleSheet (QString ("text-align : left;") );

	addAutomLayout->addWidget(addAutomButton);
	addAutomLayout->addWidget(addAutomText);

	connect (addAutomButton, &QToolButton::pressed,
			 [=] () // Lambda to create a process.
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
			parent->createProcess(process_name);
	} );
}
