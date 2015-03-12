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
        explicit EventInspectorWidget(EventModel* object, QWidget* parent = 0);

    signals:

    public slots:
        void addAddress(const QString& addr);
        void updateDisplayedValues(EventModel* obj);

        void on_addAddressClicked();
        void on_conditionChanged();

        void updateMessages();

        void removeState(QString);

    private:
        QVector<QWidget*> m_properties;

        std::vector<QWidget*> m_addresses;

        QLabel* m_date {};
        QLineEdit* m_conditionWidget {};
        QLineEdit* m_addressLineEdit {};
        QWidget* m_addressesWidget{};
        EventModel* m_model {};

        InspectorSectionWidget* m_prevConstraints;
        InspectorSectionWidget* m_nextConstraints;

        MetadataWidget* m_metadata {};
};
