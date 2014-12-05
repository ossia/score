#pragma once

#include <interface/inspector/InspectorWidgetBase.hpp>
class EventModel;

class QFormLayout;

/*!
 * \brief The EventInspectorView class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
// @todo Rename in EventInspectorWidget
class EventInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit EventInspectorWidget (EventModel* object, QWidget* parent = 0);

	signals:

	public slots:
		void addAddress(const QString& addr);
		void updateDisplayedValues (EventModel* obj);

		void on_addAddressClicked();

		void updateMessages();

	private:
		std::vector<QWidget*> m_properties;

		std::vector<QLabel*> m_addresses;

		QLineEdit* m_addressLineEdit{};
		EventModel* m_eventModel{};

};
