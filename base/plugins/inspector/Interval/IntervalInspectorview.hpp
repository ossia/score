#pragma once

#include <Inspector/InspectorWidgetInterface.hpp>
#include "objectinterval.hpp"

class QFormLayout;

/*!
 * \brief The IntervalInspectorView class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Interval (Timebox) element.
 */

class IntervalInspectorView : public InspectorWidgetInterface
{
		Q_OBJECT
	public:
		explicit IntervalInspectorView (ObjectInterval* object, QWidget* parent = 0);

	signals:

	public slots:
		/*!
		 * \brief addAutomation Add an automation
		 */
		void addAutomation (QString address = "automation");
		void updateDisplayedValues (ObjectInterval* obj);
		void reorderAutomations();

	private:

		QFormLayout* _startForm;
		QFormLayout* _endForm;

		std::vector<QWidget*>* _properties;
		std::vector<QWidget*>* _automations;

};
