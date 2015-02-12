#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>
class EventModel;

class QFormLayout;
class MetadataWidget;

/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
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
		void on_conditionChanged();

        void on_scriptingNameChanged(QString);
        void on_labelChanged(QString);
        void on_commentsChanged(QString);

		void updateMessages();

	private:
        QVector<QWidget*> m_properties;

		std::vector<QLabel*> m_addresses;

		QLineEdit* m_conditionWidget{};
		QLineEdit* m_addressLineEdit{};
		EventModel* m_eventModel{};

        MetadataWidget* m_metadata{};
};
