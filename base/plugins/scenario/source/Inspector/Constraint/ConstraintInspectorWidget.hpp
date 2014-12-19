#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>

class ConstraintModel;
class ProcessSharedModelInterface;

class QFormLayout;

/*!
 * \brief The ConstraintInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Constraint (Timebox) element.
 */
class ConstraintInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit ConstraintInspectorWidget (ConstraintModel* object, QWidget* parent = 0);

	public slots:
		/*!
		 * \brief addAutomation Add an automation
		 */
		void addAutomation (QString address = "automation");
		void updateDisplayedValues (ConstraintModel* obj);
		void reorderAutomations();

		void createProcess(QString processName);
		void displayProcess(ProcessSharedModelInterface*);

	private:
		ConstraintModel* m_currentConstraint{};

		QFormLayout* _startForm;
		QFormLayout* _endForm;

		std::vector<QWidget*> _properties;
		std::vector<QWidget*> _automations;

};
