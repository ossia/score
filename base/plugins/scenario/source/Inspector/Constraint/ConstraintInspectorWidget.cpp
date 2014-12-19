#include "ConstraintInspectorWidget.hpp"

#include "Widgets/AddProcessWidget.hpp"
#include "Widgets/AddBoxWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Commands/Constraint/AddProcessToConstraintCommand.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <InspectorInterface/InspectorSectionWidget.hpp>

#include <tools/ObjectPath.hpp>

#include <QFrame>
#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QToolButton>
#include <QPushButton>
#include <QApplication>

ConstraintInspectorWidget::ConstraintInspectorWidget (ConstraintModel* object, QWidget* parent) :
	InspectorWidgetBase (parent)
{
	setObjectName ("Constraint");

	// Processes
	m_processSection = new InspectorSectionWidget ("Processes", this);
	m_processSection->setObjectName("Processes");

	m_properties.push_back(m_processSection);
	m_properties.push_back(new SharedProcessWidget{this});

	// Separator
	QFrame* sep = new QFrame{this};
	sep->setFrameShape(QFrame::HLine);
	sep->setLineWidth(2);
	m_properties.push_back(sep);

	// Boxes
	m_boxSection = new InspectorSectionWidget{"Boxes", this};
	m_boxSection->setObjectName("Boxes");

	m_boxWidget = new BoxWidget{this};

	m_properties.push_back(m_boxSection);
	m_properties.push_back(m_boxWidget);

	// Display data
	updateSectionsView (areaLayout(), m_properties);
	areaLayout()->addStretch(1);

	updateDisplayedValues(object);
}

void ConstraintInspectorWidget::updateDisplayedValues (ConstraintModel* constraint)
{
	// Cleanup the widgets
	for(auto& process : m_processesSectionWidgets)
	{
		delete process;
	}
	m_processesSectionWidgets.clear();

	// Cleanup the connections
	for(auto& connection : m_connections)
		QObject::disconnect(connection);
	m_connections.clear();


	if (constraint != nullptr)
	{
		m_currentConstraint = constraint;

		// Constraint settings
		setName (constraint->name() );
		setColor (constraint->color() );
		setComments (constraint->comment() );
		setInspectedObject (constraint);
		changeLabelType ("Constraint");

		// Constraint interface
		m_connections.push_back(
					connect(m_currentConstraint, &ConstraintModel::processCreated,
							this,				 &ConstraintInspectorWidget::on_processCreated));
		m_connections.push_back(
					connect(m_currentConstraint, &ConstraintModel::processRemoved,
							this,				 &ConstraintInspectorWidget::on_processRemoved));
		m_connections.push_back(
					connect(m_currentConstraint, &ConstraintModel::boxCreated,
							this,				 &ConstraintInspectorWidget::on_boxCreated));
		m_connections.push_back(
					connect(m_currentConstraint, &ConstraintModel::boxRemoved,
							this,				 &ConstraintInspectorWidget::on_boxRemoved));

		// Processes
		for(ProcessSharedModelInterface* process : constraint->processes())
		{
			displaySharedProcess(process);
		}

		// Box
		m_boxWidget->setModel(m_currentConstraint);
		m_boxWidget->updateComboBox();

		for(BoxModel* box: constraint->boxes())
		{
			displayBox(box);
		}

		// Deck

		// ProcessViews
	}
	else
	{
		m_currentConstraint = nullptr;
		m_boxWidget->setModel(nullptr);
	}
}

void ConstraintInspectorWidget::createProcess(QString processName)
{
	auto cmd = new AddProcessToConstraintCommand(
						ObjectPath::pathFromObject("BaseConstraintModel",
												   m_currentConstraint),
						processName);
	emit submitCommand(cmd);

	updateDisplayedValues(m_currentConstraint);
}

void ConstraintInspectorWidget::createBox()
{
	auto cmd = new Scenario::Command::AddBoxToConstraint(
						ObjectPath::pathFromObject(
							"BaseConstraintModel",
							m_currentConstraint));
	emit submitCommand(cmd);

	updateDisplayedValues(m_currentConstraint);
}

void ConstraintInspectorWidget::createDeck(int box)
{

}

void ConstraintInspectorWidget::displaySharedProcess(ProcessSharedModelInterface* process)
{
	// TODO specialize by using custom widgets provided by the processes.
	// Also handle the case where a process does not.
	InspectorSectionWidget* newProc = new InspectorSectionWidget (process->processName());
	m_processesSectionWidgets.push_back (newProc);
	m_processSection->addContent (newProc);
}

void ConstraintInspectorWidget::displayBox(BoxModel* box)
{
	InspectorSectionWidget* newBox = new InspectorSectionWidget{QString{"Box.%1"}.arg(box->id())};
	m_boxesSectionWidgets.push_back(newBox);
	m_boxSection->addContent(newBox);
}

void ConstraintInspectorWidget::displayDeck(StoreyModel*)
{

}

void ConstraintInspectorWidget::on_processCreated(QString processName, int processId)
{
	updateDisplayedValues(m_currentConstraint);
}

void ConstraintInspectorWidget::on_processRemoved(int processId)
{
	updateDisplayedValues(m_currentConstraint);
}

void ConstraintInspectorWidget::on_boxCreated(int viewId)
{
	m_boxWidget->updateComboBox();
	updateDisplayedValues(m_currentConstraint);
}

void ConstraintInspectorWidget::on_boxRemoved(int viewId)
{
	m_boxWidget->updateComboBox();
	updateDisplayedValues(m_currentConstraint);
}
