#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
class EventModel;

class QFormLayout;
class StateModel;
class MetadataWidget;
struct Message;


/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
class EventInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit EventInspectorWidget(
                const EventModel* object,
                QWidget* parent = 0);

        void addState(const StateModel* state);
        void focusState(const StateModel* state);

    public slots:
        void updateDisplayedValues(const EventModel* obj);

        void on_conditionChanged();
        void on_triggerChanged();

        void updateInspector();
        void modelDateChanged();

    private:
        QVector<QWidget*> m_properties;

        std::vector<QWidget*> m_states;

        QLabel* m_date {};
        QLineEdit* m_conditionLineEdit{};
        QLineEdit* m_triggerLineEdit{};
        QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel* m_model{};

        MetadataWidget* m_metadata {};
};
