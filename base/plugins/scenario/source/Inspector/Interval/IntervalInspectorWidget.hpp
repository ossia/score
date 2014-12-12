#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>

class IntervalModel;

class QFormLayout;

/*!
 * \brief The IntervalInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Interval (Timebox) element.
 */
class IntervalInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit IntervalInspectorWidget (IntervalModel* object, QWidget* parent = 0);

	signals:

	public slots:
		/*!
		 * \brief addAutomation Add an automation
		 */
		void addAutomation (QString address = "automation");
		void updateDisplayedValues (IntervalModel* obj);
		void reorderAutomations();

	private:

		QFormLayout* _startForm;
		QFormLayout* _endForm;

		std::vector<QWidget*>* _properties;
		std::vector<QWidget*>* _automations;

};
