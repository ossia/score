#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>
class TimeNodeModel;

class QFormLayout;

/*!
 * \brief The TimeNodeInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an TimeNode (Timebox) element.
 */
class TimeNodeInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit TimeNodeInspectorWidget (TimeNodeModel* object, QWidget* parent = 0);

	signals:

	public slots:
		void updateDisplayedValues (TimeNodeModel* obj);

        void updateInspector();

	private:
		std::vector<QWidget*> m_properties;
        std::vector<QLabel*> m_events;

		TimeNodeModel* m_timeNodeModel{};

        InspectorSectionWidget* m_eventList{};
        QLabel* m_date{};
};
